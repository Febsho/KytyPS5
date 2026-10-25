#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
  Loader::SymbolDatabase symbols;
  Libs::InitAll(&symbols);

  const auto *stdout_symbol =
      symbols.FindByNid("zqJhBxAKfsc", Loader::SymbolType::Object);
  const auto *stderr_symbol =
      symbols.FindByNid("as8Od-tH1BI", Loader::SymbolType::Object);
  const auto points_to_stream = [](const Loader::SymbolRecord *symbol,
                                   FILE *stream) {
    return symbol != nullptr && symbol->vaddr != 0 &&
           *reinterpret_cast<FILE **>(static_cast<uintptr_t>(symbol->vaddr)) ==
               stream;
  };
  if (!points_to_stream(stdout_symbol, stdout) ||
      !points_to_stream(stderr_symbol, stderr)) {
    std::fprintf(
        stderr,
        "LibcInternal stdio data imports are not bound to host stream slots\n");
    return 1;
  }

  const auto* getenv_symbol =
      symbols.FindByNid("smbQukfxYJM", Loader::SymbolType::Func);
  if (getenv_symbol == nullptr) {
    std::fprintf(stderr, "LibcInternal getenv import does not resolve\n");
    return 1;
  }
  const auto guest_getenv = reinterpret_cast<char*(KYTY_SYSV_ABI*)(const char*)>(
      static_cast<uintptr_t>(getenv_symbol->vaddr));
  const char* environment_names[] = {
      "PATH", "KYTY_TEST_ENV_VAR_DOES_NOT_EXIST_7D3A"};
  for (const char* name : environment_names) {
    const char* expected = std::getenv(name);
    char*       actual   = guest_getenv(name);
    if ((expected == nullptr) != (actual == nullptr) ||
        (expected != nullptr && std::strcmp(expected, actual) != 0)) {
      std::fprintf(stderr,
                   "LibcInternal getenv result disagrees with host environment\n");
      return 1;
    }
  }

  return 0;
}
