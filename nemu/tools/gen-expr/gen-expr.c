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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int choose(int n) {
  return rand() % n;
}

static void gen(char c) {
  size_t len = strlen(buf);
  if (len < sizeof(buf) - 1) {
    buf[len] = c;
    buf[len + 1] = '\0';
  }
}

// static void gen_rand_op() {
//   switch (choose(4)) {
//     case 0: 
//       gen('+'); 
//       break;
//     case 1:  
//       gen('-'); 
//       break;
//     case 2:  
//       gen('*'); 
//       break;
//     default: 
//       gen('/'); 
//       break;
//   }
// }


static void gen_num(int num) {
  char temp[32];
  sprintf(temp, "%d", num);
  strcat(buf, temp);
}

static void gen_space(){
  //30% chance to add space
  if (choose(3) == 0) {
    gen(' ');
  }
}

static void gen_rand_expr();

/* ------------------ 核心修改点 1 ------------------ */
// 专门用来生成绝对不为 0 的表达式
static void gen_rand_expr_nonzero() {
  switch (choose(3)) {
    case 0: 
      gen_num(choose(99) + 1); // 生成 1 ~ 99 的常数，确保非 0
      gen_space();
      break;
    case 1:  
      gen('('); 
      gen_space();
      gen_rand_expr_nonzero(); // 递归生成非零表达式
      gen_space();
      gen(')'); 
      gen_space();
      break;
    default: 
      // 对于复合表达式，最安全且简单的方法是强制左边是非零，右边加上它
      // A + B (如果 A, B 都是正数，结果必定非零)
      gen_rand_expr_nonzero(); 
      gen_space();
      gen('+'); 
      gen_space();
      gen_rand_expr_nonzero(); 
      gen_space();
      break;
  }
}

/* ------------------ 核心修改点 2 ------------------ */
static void gen_rand_expr() {
  switch (choose(3)) {
    case 0: 
      gen_num(choose(100)); // 这里可以生成 0
      gen_space();
      break;
    case 1:  
      gen('('); 
      gen_space();
      gen_rand_expr(); 
      gen_space();
      gen(')'); 
      gen_space();
      break;
    default: {
      int op = choose(4); // 决定生成哪种操作符
      
      gen_rand_expr(); 
      gen_space();
      
      if (op == 3) { // 如果是除法 '/'
        gen('/');
        gen_space();
        gen_rand_expr_nonzero(); // 右侧强制生成非零表达式！
      } else {
        if (op == 0) gen('+');
        else if (op == 1) gen('-');
        else gen('*');
        gen_space();
        gen_rand_expr(); // 其他操作符左右都可以是任意表达式
      }
      gen_space();
      break;
    }
  }
}



int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf[0] = '\0';
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
