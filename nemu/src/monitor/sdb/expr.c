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
    Log("tokens[%d] = %s", i, tokens[i].str);
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
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  }
  else {
    int op;
    for ( op = p; op < q; op++){
      if (tokens[op].type == '+' || tokens[op].type == '-' || tokens[op].type == '*' || tokens[op].type == '/')
        break;
    }
    
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);
    int op_type = tokens[op].type;
    switch (op_type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': return val1 / val2;
      default: assert(0);
    }
  }
}

static int check_parentheses(int p, int q) {
  // check if the expression is surrounded by a matched pair of parentheses
  if (p > q)
    return -1;
  int cnt = 0, i;
  for (i = p; i <= q; ++i) {
    if (tokens[i].type == TK_LP)
      ++cnt;
    else if (tokens[i].type == TK_RP)
      --cnt;
    if (cnt < 0)
      return 0;
  }
  if (cnt)
    return 0;
  if (tokens[p].type != TK_LP || tokens[q].type != TK_RP)
    return -1;
  int result = check_parentheses(p + 1, q - 1);
  if (result) /* value is -1 or 1 */
    return 1;
  else
    return -1;
}

// TODO
// static void find_unary_op(){
//   //标记一元操作符
//   for (int i = 0; i < nr_token; i++){
    
//     /* code */
//   }
  
// }
