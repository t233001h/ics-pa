#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/monitor/sdb/expr.c" // 包含表达式求值代码

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Failed to open input file");
        return 1;
    }

    char line[1024];
    int total = 0, passed = 0, failed = 0;

    while (fgets(line, sizeof(line), fp)) {
        total++;
        unsigned expected;
        char expr_str[512];
        
        // 解析行："结果 表达式"
        if (sscanf(line, "%u %[^\n]", &expected, expr_str) != 2) {
            fprintf(stderr, "Invalid line: %s", line);
            continue;
        }

        // 计算表达式
        bool success = false;
        word_t result = expr(expr_str, &success);

        // 验证结果
        if (success && result == expected) {
            passed++;
        } else {
            failed++;
            fprintf(stderr, "Failed: %s\nExpected: %u, Got: %u, Success: %d\n", 
                    expr_str, expected, result, success);
        }
    }

    fclose(fp);

    // 输出测试结果
    printf("Total: %d, Passed: %d, Failed: %d\n", total, passed, failed);
    printf("Accuracy: %.2f%%\n", (double)passed / total * 100);

    return failed == 0 ? 0 : 1;
}
