#ifndef CHECK_EXT_H
#define CHECK_EXT_H

#include <check.h>
#include <stdlib.h> /* getenv */

/* 	redefine env
 CK_DEFAULT_TIMEOUT=<sec> to set timeout
 CK_TIMEOUT_MULTIPLIER=<n> to multiply all timeouts
 CK_FORK=no|yes(default)
 CK_VERBOSITY={silent|minimal|normal(default)|verbose}
 added here:
 CK_ONLY_CASE=X
 CK_ONLY_TEST=X
 */

/*
 * Example of suite declaration:
 int main(void){
 return RUN_SUITE(
 START_SUITE(test1){
 ADD_CASE(
 START_CASE(smoke){
 FIXTURE(_setup1, _teardown1, 1);
 TIMEOUT(10);
 ADD_TEST(test_xxx);
 ADD_TEST(test_yyy);
 }END_CASE
 );
 ADD_CASE(
 START_CASE(core){
 FIXTURE(_setup2, _teardown2, 1);
 TIMEOUT(5);
 ADD_TEST(test_div_by_zero);
 ADD_TEST(test_div_by_zero1);
 }END_CASE
 );
 }END_SUITE
 );
 }
 */

#ifndef CHECK_EXT_NO_RE
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#endif /*CHECK_EXT_NO_RE*/

/*
 * Sometimes it's necessary to generate errors
 * (to show that it's error inside check library)
 */
void eprintf(const char *fmt, const char *file, int line, ...);

#define ck_error(message, args...) \
do { \
	mark_point(); \
	eprintf(message,__FILE__,__LINE__, ##args); \
}while(0)

#define START_SUITE(_suite_) \
	({ \
		Suite *suite = NULL; \
		if (1) { \
			suite = suite_create(#_suite_);

#define ADD_CASE(_case_) \
	do { \
		TCase *tcase; tcase = _case_; \
		if (tcase) \
			suite_add_tcase(suite, tcase); \
	}while(0)

#define END_SUITE \
		} \
		suite; \
	})

#define START_CASE(_case_) \
	({ \
		TCase *tcase = NULL; \
		char *only_case = getenv("CK_ONLY_CASE"); \
		if (!only_case || strncmp(only_case, #_case_, sizeof(#_case_)) == 0){ \
			tcase = tcase_create(#_case_); \

#define FIXTURE(_setup_, _teardown_) \
		tcase_add_checked_fixture(tcase, _setup_, _teardown_); \

#define FIXTURE_UNCHECKED(_setup_, _teardown_) \
		tcase_add_unchecked_fixture(tcase, _setup_, _teardown_)

#define TIMEOUT(_timeout_) \
		tcase_set_timeout(tcase, _timeout_)

/**
 * if _signal_hdlr_ is not NULL it will be set as signal handler function
 * if function calls exit it's argument MUST be equal to _expected_exit_val_
 * test will be called in loop for(i = _start_; i < _end_; i++)
 */
#define _ADD_TEST(_test_, _signal_hdlr_, _expected_exit_val_, _start_, _end_) \
	do { \
		char *only_test = getenv("CK_ONLY_TEST"); \
		if ((!only_test || strncmp(only_test, #_test_, sizeof(#_test_)) == 0)) \
			_tcase_add_test(tcase, _test_, "" #_test_ "", _signal_hdlr_, _expected_exit_val_, _start_, _end_); \
	}while(0)

#define ADD_TEST(_test_) \
		_ADD_TEST(_test_, (0), (0), 0, 1)

#define END_CASE \
		} \
		tcase; \
	})

int static inline run_suite(Suite *suite) {
    int nr_failed = 0;
    SRunner *sr = srunner_create(suite);
    srunner_set_fork_status(sr, CK_FORK_GETENV);
    srunner_run_all(sr, CK_ENV);
    nr_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return nr_failed;
}

#ifndef CHECK_EXT_NO_RE

static inline
int regex_match(const char *regex, const char *string) {
    regex_t r;

    int result;
    result = regcomp(&r, (regex), REG_EXTENDED);
    if (result != 0) {
        char errbuffer[128];
        regerror(result, &r, errbuffer, sizeof(errbuffer));
        ck_error(errbuffer);
    }

    result = regexec(&r, (string), 0, NULL, 0);

    regfree(&r);

    if (result == REG_ESPACE) {
        ck_error("Regex matching ran out of memory");
    }

    if (result == REG_NOMATCH)
        return 1;

    return 0;
}

#define assert_string_match(string, regex) \
    if (regex_match((regex), (string)) != 0) \
        fail("String '%s' expected to match regex '%s', but it doesn't", (string), (regex));

#define assert_string_doesnot_match(string, regex) \
    if (regex_match((regex), (string)) == 0) \
        fail("String '%s' expected not to match regex '%s', but it does", (string), (regex));

#endif /*CHECK_EXT_NO_RE*/

#endif /*CHECK_EXT_H*/
