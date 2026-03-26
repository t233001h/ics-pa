/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_NUM, TK_HEX, TK_REG, // 数字和寄存器
  TK_MINUS, TK_MUL, TK_DIV, // 运算符
  //TODO TK_NEG, TK_DEREF, TK_NOT, // 一元运算符
  TK_LP, TK_RP, // 括号


};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // 空格
  {"==", TK_EQ},        // 等于
  {"\\+", '+'},         // 加号
  {"-", '-'},      // 减号
  {"\\*", '*'},      // 乘号
  {"/", '/'},        // 除号
  {"\\(", TK_LP},       // 左括号
  {"\\)", TK_RP},       // 右括号
  {"0x[0-9a-fA-F]+", TK_HEX}, // 十六进制数字
  {"[0-9]+", TK_NUM},   // 十进制数字
  {"\\$((0)|(ra|sp|gp|tp)|(t[0-6])|(s([0-9]|1[01]))|(a[0-7]))", TK_REG}, // 寄存器
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static int check_parentheses(int p, int q);
word_t eval(int p, int q);


static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

 if (rules[i].token_type != TK_NOTYPE) {
          // 确保不超过 tokens 数组大小
          if (nr_token >= 32) {
            printf("Too many tokens\n");
            return false;
          }
          // 设置 token 类型
          tokens[nr_token].type = rules[i].token_type;
          // 复制字符串到 token.str
          if (substr_len >= 32) {
            // 截断过长的字符串
            strncpy(tokens[nr_token].str, substr_start, 31);
            tokens[nr_token].str[31] = '\0';
          } else {
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
          }
          nr_token++;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%.*s^\n", position, e, position, "");
      return false;
    }
  }
// 打印 tokens 数组，一行输出整个字符串
  for (i = 0; i < nr_token; i ++) {
    printf("%s", tokens[i].str);
  }
  printf("\n");
  
  return true;
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  word_t result = eval(0, nr_token - 1);
  *success = true; 
  return result;
}



/* 寻找主运算符的索引 */
int find_main_op(int p, int q) {
    int op = -1;
    int min_priority = 100; // 初始设为一个较大的值
    int nested = 0;

    for (int i = p; i <= q; i++) {
        // 1. 处理括号嵌套，主运算符必须在括号外
        if (tokens[i].type == '(') {
            nested++;
            continue;
        }
        if (tokens[i].type == ')') {
            nested--;
            continue;
        }
        if (nested > 0) continue;

        // 2. 获取当前 token 的优先级
        int cur_priority = 0;
        switch (tokens[i].type) {
            case '+': case '-': 
                cur_priority = 1; break;
            case '*': case '/': 
                cur_priority = 2; break;
            // 后期可以在这里轻松扩展：
            // case TK_EQ: case TK_NEQ: cur_priority = 0; break;
            default: 
                continue; // 不是运算符，跳过
        }

        /* 3. 比较优先级确定主运算符
         * 注意：对于左结合运算符（+ - * /），同优先级时选右边的。
         * 所以使用 <= min_priority。
         * 如果以后加入右结合运算符（如单目 -），同优先级时则不能更新 op。
         */
        if (cur_priority <= min_priority) {
            min_priority = cur_priority;
            op = i;
        }
    }
    return op;
}

word_t eval(int p, int q) {
  if (p > q) {
    printf("Bad expression\n");
    assert(0);
    /* Bad expression */
  }
  else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
     long long num;
    if (tokens[p].type == TK_HEX)
      sscanf(tokens[p].str, "%llx", &num);
    else if (tokens[p].type == TK_NUM)
      sscanf(tokens[p].str, "%lld", &num);
    else if (tokens[p].type == TK_REG) {
      bool success = true;
      int result = isa_reg_str2val(tokens[p].str, &success);
      if (!success) {
        printf("ERROR: invalid register %s\n", tokens[p].str);
        assert(0);
      }
      return result;
    }
    else {
      printf("ERROR: invalid token %d (enum index)\n", tokens[p].type);
      assert(0);
    }
    if (num > 0x100000000) {
      printf("WARNING: the input number [%s] is too large", tokens[p].str);
      assert(0);
    }
    return (word_t)num;
  }

  // --- 修改点：优先处理括号脱壳 ---
  else if (check_parentheses(p, q) == true) {
    /* 比如 ((38))，脱壳变成 (38)，再次递归变成 38 */
    return eval(p + 1, q - 1);
  }
  else {
    // --- 修改点：找主运算符 ---
    int op = find_main_op(p, q);
    
    if (op == -1) {
        // 如果既不是单数字，又没法脱壳，还找不到运算符，才是真的坏了
        printf("Bad expression at range [%d, %d]\n", p, q);
        assert(0);
    }

    // 递归计算左右两边
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': 
        if (val2 == 0) { printf("Div by zero\n"); assert(0); }
        return val1 / val2;
      default: assert(0);
    }
  }
}

static int check_parentheses(int p, int q) {
  // 1. 基本检查：首尾必须是左括号和右括号
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  // 2. 检查首尾括号是否是相互匹配的一对
  int cnt = 0;
  for (int i = p; i < q; i++) { // 注意：只循环到 q-1
    if (tokens[i].type == '(') {
      cnt++;
    } else if (tokens[i].type == ')') {
      cnt--;
    }
    
    // 关键点：如果在还没扫到最后一个 token 时 cnt 就归零了
    // 说明 tokens[p] 的左括号在中间就匹配完了
    // 例子：(1+2)*(3+4)，当 i 扫到 2 后面的 ')' 时，cnt 变 0，返回 false
    if (cnt == 0) {
      return false;
    }
  }

  // 3. 如果循环结束 cnt == 1，说明 tokens[p] 正好对应 tokens[q]
  return (cnt == 1);
}

// TODO
// static void find_unary_op(){
//   //标记一元操作符
//   for (int i = 0; i < nr_token; i++){
    
//     /* code */
//   }
  
// }
