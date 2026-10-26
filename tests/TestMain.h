#pragma once

// 외부 의존성 없는 최소 테스트 도구. 테스트가 늘어나면 Catch2/GoogleTest 도입을 검토한다.

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace naru::test {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

inline int runAll() {
    int failedCases = 0;
    for (const auto& c : registry()) {
        int before = failures();
        c.body();
        bool passed = failures() == before;
        std::printf("[%s] %s\n", passed ? "PASS" : "FAIL", c.name);
        if (!passed) {
            ++failedCases;
        }
    }
    std::printf("\n%zu cases, %d failed\n", registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}

} // namespace naru::test

#define NARU_CONCAT_INNER(a, b) a##b
#define NARU_CONCAT(a, b) NARU_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                      \
    static void NARU_CONCAT(naru_test_, __LINE__)();                                         \
    static ::naru::test::Registrar NARU_CONCAT(naru_reg_, __LINE__)(name,                   \
                                                                    NARU_CONCAT(naru_test_, __LINE__)); \
    static void NARU_CONCAT(naru_test_, __LINE__)()

#define CHECK(expr)                                                                          \
    do {                                                                                     \
        if (!(expr)) {                                                                       \
            std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr);          \
            ++::naru::test::failures();                                                      \
        }                                                                                    \
    } while (0)

#define TEST_MAIN()                                                                          \
    int main() { return ::naru::test::runAll(); }
