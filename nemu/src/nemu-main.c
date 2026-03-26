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

#include <common.h>
#include <stdio.h>
#include <string.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

// Forward declaration for expr function
word_t expr(char *e, bool *success);

static void test_expr_eval();

int main(int argc, char *argv[]) {
  /* Test expression evaluation */
  if (argc == 2 && strcmp(argv[1], "--test-expr") == 0) {
    test_expr_eval();
    return 0;
  }

  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif

  /* Start engine. */
  engine_start();

  return is_exit_status_bad();
}

static void test_expr_eval() {
  // 初始化正则表达式
  extern void init_regex();
  init_regex();

  FILE *fp = fopen("tools/gen-expr/input", "r");
  if (!fp) {
    fprintf(stderr, "Failed to open input file\n");
    return;
  }

  char line[1024];
  int total = 0, passed = 0, failed = 0;

  while (fgets(line, sizeof(line), fp)) {
    total++;
    unsigned expected;
    char expr_str[512];
    
    // 正确的格式字符串，确保在一行内
    if (sscanf(line, "%u %[^\n]", &expected, expr_str) != 2) {
      fprintf(stderr, "Invalid line: %s", line);
      continue;
    }

    bool success = false;
    word_t result = expr(expr_str, &success);

    if (success && result == expected) {
      //打印表达式和通过信息
      printf("Passed: %s\n", expr_str);
      passed++;
    } else {
      failed++;
      fprintf(stderr, "Failed: %s\nExpected: %u, Got: %u, Success: %d\n", 
              expr_str, expected, result, success);
    }
  }

  fclose(fp);

  printf("Total: %d, Passed: %d, Failed: %d\n", total, passed, failed);
  printf("Accuracy: %.2f%%\n", (double)passed / total * 100);
}