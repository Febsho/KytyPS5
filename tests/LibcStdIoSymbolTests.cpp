#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <cstdint>
#include <cstdio>

int main() {
  Loader::SymbolDatabase symbols;
  Libs::InitAll(&symbols);

  if (symbols.FindByNid("6kPnj51T62Y", Loader::SymbolType::Func) == nullptr) {
    std::fprintf(stderr, "VideoOut resolution status import does not resolve\n");
    return 1;
  }

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

  return 0;
}
