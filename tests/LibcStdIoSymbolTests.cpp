#include "libs/libs.h"
#include "libs/guestPrintf.h"
#include "libs/vaContext.h"
#include "loader/symbolDatabase.h"

#include <cstdint>
#include <cstdio>
#include <string_view>

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

  const auto snprintf = Libs::GetGuestSnprintfCtxFunc();
  const auto run_snprintf = [snprintf](char *destination, size_t size) {
    Libs::VaContext context{};
    context.reg_save_area.gp[0] = reinterpret_cast<uintptr_t>(destination);
    context.reg_save_area.gp[1] = size;
    context.reg_save_area.gp[2] =
        reinterpret_cast<uintptr_t>("%s:%d");
    context.reg_save_area.gp[3] =
        reinterpret_cast<uintptr_t>("hello");
    context.reg_save_area.gp[4] = 42;
    context.va_list.reg_save_area = &context.reg_save_area;
    return snprintf(&context);
  };

  char truncated[5] = {'x', 'x', 'x', 'x', 'x'};
  if (run_snprintf(truncated, sizeof(truncated)) != 8 ||
      std::string_view(truncated) != "hell") {
    std::fprintf(stderr, "guest snprintf truncation behavior is incorrect\n");
    return 1;
  }
  char empty[1] = {'x'};
  if (run_snprintf(empty, sizeof(empty)) != 8 || empty[0] != '\0' ||
      run_snprintf(nullptr, 0) != 8) {
    std::fprintf(stderr, "guest snprintf zero-sized behavior is incorrect\n");
    return 1;
  }

  return 0;
}
