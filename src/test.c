#ifdef TEST

#include "lib.h"
#include <stdio.h>

#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define BOLD "\033[1m"

void test_parse_term__empty();
void test_parse_term__only_semicolon();
void test_parse_term__only_pipe();
void test_parse_term__only_amp();
void test_parse_term__one_char_name();
void test_parse_term__no_arg();
void test_parse_term__one_char_arg();
void test_parse_term__one_arg_semi();
void test_parse_term__pipe__error();
void test_parse_term__basic_pipe();
void test_parse_term__basic_pipe_with_amp_nospace();
void test_parse_term__basic_pipe_with_amp_space();

int main() {
  test_parse_term__empty();
  test_parse_term__only_semicolon();
  test_parse_term__only_pipe();
  test_parse_term__only_amp();
  test_parse_term__one_char_name();
  test_parse_term__no_arg();
  test_parse_term__one_char_arg();
  test_parse_term__one_arg_semi();
  test_parse_term__pipe__error();
  test_parse_term__basic_pipe();
  test_parse_term__basic_pipe_with_amp_nospace();
  test_parse_term__basic_pipe_with_amp_space();

  printf(BOLD GREEN "All tests passed\n" RESET);
  return 0;
}

#endif
