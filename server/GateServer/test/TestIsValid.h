

#include "util.h"

void TestIsValid() {
    // 测试 email 格式
//    assert(isValidEmail("fasljf@fjll."));
//    assert(isValidEmail("fj11@.cm"));
//    assert(isValidEmail("fj11@cm"));
    assert(isValidEmail("123f@asd.c"));

    // 测试 username
    assert(isValidUsername("wellwei"));
//    assert(isValidUsername("90fs_"));
//    assert(isValidUsername("i23.322"));
    assert(isValidUsername("j123123"));

    // 测试 password
    assert(isValidPassword("12j34J56_+++#"));
    assert(isValidPassword("jifaOd12312."));
    assert(isValidPassword("Jj12332("));
}