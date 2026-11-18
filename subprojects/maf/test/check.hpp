/*
 * Copyright (c) 2023 MotionSpell
 * Licensed under the License terms and conditions for use, reproduction,
 * and distribution of 5GMAG software (the “License”).
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at https://www.5g-mag.com/license .
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

// Minimal test helpers: no dependency, one executable per test file, a non-zero exit code on failure.

#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace check {

struct Case {
    const char *name;
    std::function<void()> fn;
};

inline std::vector<Case> &cases()
{
    static std::vector<Case> c;
    return c;
}

inline int &failures()
{
    static int f = 0;
    return f;
}

struct Register {
    Register(const char *name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};

inline int run()
{
    for (auto &c : cases()) {
        int before = failures();
        try {
            c.fn();
        } catch (const char *e) {
            std::fprintf(stderr, "  unexpected exception: %s\n", e);
            failures()++;
        } catch (...) {
            std::fprintf(stderr, "  unexpected exception\n");
            failures()++;
        }
        std::printf("%s %s\n", failures() == before ? "PASS" : "FAIL", c.name);
    }
    return failures() == 0 ? 0 : 1;
}

} // namespace check

#define CHECK_CONCAT_(a, b) a##b
#define CHECK_CONCAT(a, b) CHECK_CONCAT_(a, b)
#define TEST_CASE(name)                                                                  \
    static void CHECK_CONCAT(test_fn_, __LINE__)();                                      \
    static check::Register CHECK_CONCAT(test_reg_, __LINE__)(name, CHECK_CONCAT(test_fn_, __LINE__)); \
    static void CHECK_CONCAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::fprintf(stderr, "  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
            check::failures()++;                                                         \
        }                                                                                \
    } while (0)

#define CHECK_THROWS(expr)                                                               \
    do {                                                                                 \
        bool thrown_ = false;                                                            \
        try { expr; } catch (...) { thrown_ = true; }                                    \
        if (!thrown_) {                                                                  \
            std::fprintf(stderr, "  %s:%d: %s did not throw\n", __FILE__, __LINE__, #expr); \
            check::failures()++;                                                         \
        }                                                                                \
    } while (0)
