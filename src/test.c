#include "lib.h"
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define BOLD "\033[1m"

void run_tests();
void tests_failed(int signum);

#ifdef TEST // CODEGEN_DEFINE_TESTS_START
void test_parse_term__empty();
void test_parse_term__only_semicolon();
void test_parse_term__only_pipe();
void test_parse_term__only_amp();
void test_parse_term__one_char_name();
void test_parse_term__no_arg();
void test_parse_term__one_char_arg();
void test_parse_term__one_arg_semi();
void test_parse_term__pipe__error();
void test_parse_term__space_amp();
void test_parse_term__basic_pipe();
void test_parse_term__basic_pipe_with_amp_nospace();
void test_parse_term__basic_pipe_with_amp_space();
void test_parse_term__amp_not_terminal();
void test_parse_cmd__basic_echo();
void test_parse_cmd__2_echos();
void test_parse_cmd__pipe_as_last();
void test_parse_cmd__anything_after_semicolon();
void test_parse_cmd__pipe_after_semi();
void test_parse_cmd__pipe_after_pipe();
void test_parse_cmd__amp_after_pipe();
void test_parse_cmd__2_echos_amp();
#endif // CODEGEN_DEFINE_TESTS_END

int main() {
  signal(SIGABRT, tests_failed);
  run_tests();
  return 0;
}

void run_tests() {
#ifdef TEST // CODEGEN_RUN_TESTS_START
  test_parse_term__empty();
  test_parse_term__only_semicolon();
  test_parse_term__only_pipe();
  test_parse_term__only_amp();
  test_parse_term__one_char_name();
  test_parse_term__no_arg();
  test_parse_term__one_char_arg();
  test_parse_term__one_arg_semi();
  test_parse_term__pipe__error();
  test_parse_term__space_amp();
  test_parse_term__basic_pipe();
  test_parse_term__basic_pipe_with_amp_nospace();
  test_parse_term__basic_pipe_with_amp_space();
  test_parse_term__amp_not_terminal();
  test_parse_cmd__basic_echo();
  test_parse_cmd__2_echos();
  test_parse_cmd__pipe_as_last();
  test_parse_cmd__anything_after_semicolon();
  test_parse_cmd__pipe_after_semi();
  test_parse_cmd__pipe_after_pipe();
  test_parse_cmd__amp_after_pipe();
  test_parse_cmd__2_echos_amp();
#endif // CODEGEN_RUN_TESTS_END

  fprintf(stderr, BOLD GREEN "All tests passed\n" RESET);
}

void tests_failed(int signum) {
  if (signum != SIGABRT) {
    errx(EXIT_FAILURE, "Tests are corrupted. SIGABRT handler got not SIGABRT");
  }
  fprintf(stderr, BOLD RED "Some test failed\n" RESET);
  exit(EXIT_FAILURE);
}
