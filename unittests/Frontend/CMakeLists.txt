# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

add_executable(CompilerInstanceTest CompilerInstanceTest.cpp ${CANGJIE_SRC_OBJECTS})
target_link_libraries(
    CompilerInstanceTest
    ${LINK_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME CompilerInstanceTest COMMAND CompilerInstanceTest)

add_executable(ASTReleaseTest ASTReleaseTest.cpp ${CANGJIE_SRC_OBJECTS})
target_link_libraries(
    ASTReleaseTest
    ${LINK_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ASTReleaseTest COMMAND ASTReleaseTest)

add_executable(ASTReleaseFunctionalTest ASTReleaseFunctionalTest.cpp ${CANGJIE_SRC_OBJECTS})
target_link_libraries(
    ASTReleaseFunctionalTest
    ${LINK_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ASTReleaseFunctionalTest COMMAND ASTReleaseFunctionalTest)
