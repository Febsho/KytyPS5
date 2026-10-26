#include "libs/guestPrintf.h"
#include "libs/guestStdio.h"
#include "libs/libs.h"
#include "libs/vaContext.h"
#include "loader/symbolDatabase.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

int main() {
  Loader::SymbolDatabase symbols;
  Libs::InitAll(&symbols);

  if (symbols.FindByNid("6kPnj51T62Y", Loader::SymbolType::Func) == nullptr) {
    std::fprintf(stderr,
                 "VideoOut resolution status import does not resolve\n");
    return 1;
  }

  const auto *stdout_symbol =
      symbols.FindByNid("2sWzhYqFH4E", Loader::SymbolType::Object);
  const auto *legacy_stdout_symbol =
      symbols.FindByNid("zqJhBxAKfsc", Loader::SymbolType::Object);
  const auto *stderr_symbol =
      symbols.FindByNid("as8Od-tH1BI", Loader::SymbolType::Object);
  const auto guest_file = [](const Loader::SymbolRecord *symbol) {
    if (symbol == nullptr || symbol->vaddr == 0) {
      return static_cast<Libs::GuestStdio::File *>(nullptr);
    }
    return *reinterpret_cast<Libs::GuestStdio::File **>(
        static_cast<uintptr_t>(symbol->vaddr));
  };
  auto *guest_stdout = guest_file(stdout_symbol);
  auto *legacy_stdout = guest_file(legacy_stdout_symbol);
  auto *guest_stderr = guest_file(stderr_symbol);
  const auto valid_stream = [](const Libs::GuestStdio::File *stream,
                               int32_t descriptor) {
    return stream != nullptr && stream->descriptor == descriptor &&
           stream->flags == Libs::GuestStdio::kWritableUnbufferedFlags &&
           stream->buffer_base != nullptr &&
           stream->write_pointer == stream->buffer_base &&
           stream->buffer_base < stream->write_limit &&
           stream->write_limit == stream->buffer_limit;
  };
  if (stdout_symbol == nullptr || legacy_stdout_symbol == nullptr ||
      stdout_symbol->vaddr != legacy_stdout_symbol->vaddr ||
      guest_stdout == reinterpret_cast<Libs::GuestStdio::File *>(stdout) ||
      guest_stderr == reinterpret_cast<Libs::GuestStdio::File *>(stderr) ||
      !valid_stream(guest_stdout, 1) || !valid_stream(legacy_stdout, 1) ||
      !valid_stream(guest_stderr, 2)) {
    std::fprintf(stderr, "LibcInternal stdio imports do not expose "
                         "guest-layout stream objects\n");
    return 1;
  }

  std::FILE *temporary = std::tmpfile();
  if (temporary == nullptr) {
    std::fprintf(
        stderr,
        "could not create a temporary file for guest stdio validation\n");
    return 1;
  }
  Libs::GuestStdio::Stream test_stream(2);
  constexpr char output[] = "guest stdio flush test\n";
  std::memcpy(test_stream.file.write_pointer, output, sizeof(output) - 1);
  test_stream.file.write_pointer += sizeof(output) - 1;
  char readback[sizeof(output)] = {};
  const bool flush_ok =
      Libs::GuestStdio::Flush(&test_stream.file, temporary) == 0 &&
      std::fseek(temporary, 0, SEEK_SET) == 0 &&
      std::fread(readback, 1, sizeof(output) - 1, temporary) ==
          sizeof(output) - 1 &&
      std::string_view(readback) ==
          std::string_view(output, sizeof(output) - 1) &&
      test_stream.file.write_pointer == test_stream.file.buffer_base;
  std::fclose(temporary);
  if (!flush_ok) {
    std::fprintf(stderr, "guest stdio buffer did not flush correctly\n");
    return 1;
  }

  const auto *fflush_symbol =
      symbols.FindByNid("MUjC4lbHrK4", Loader::SymbolType::Func);
  if (fflush_symbol == nullptr) {
    std::fprintf(stderr, "LibcInternal fflush import does not resolve\n");
    return 1;
  }
  const auto guest_fflush =
      reinterpret_cast<int(KYTY_SYSV_ABI *)(Libs::GuestStdio::File *)>(
          static_cast<uintptr_t>(fflush_symbol->vaddr));
  if (guest_fflush(nullptr) != 0) {
    std::fprintf(
        stderr,
        "LibcInternal fflush(nullptr) did not flush standard streams\n");
    return 1;
  }

  const auto *getenv_symbol =
      symbols.FindByNid("smbQukfxYJM", Loader::SymbolType::Func);
  if (getenv_symbol == nullptr) {
    std::fprintf(stderr, "LibcInternal getenv import does not resolve\n");
    return 1;
  }
  const auto guest_getenv =
      reinterpret_cast<char *(KYTY_SYSV_ABI *)(const char *)>(
          static_cast<uintptr_t>(getenv_symbol->vaddr));
  const char *environment_names[] = {"PATH",
                                     "KYTY_TEST_ENV_VAR_DOES_NOT_EXIST_7D3A"};
  for (const char *name : environment_names) {
    const char *expected = std::getenv(name);
    char *actual = guest_getenv(name);
    if ((expected == nullptr) != (actual == nullptr) ||
        (expected != nullptr && std::strcmp(expected, actual) != 0)) {
      std::fprintf(
          stderr,
          "LibcInternal getenv result disagrees with host environment\n");
      return 1;
    }
  }

  const auto snprintf = Libs::GetGuestSnprintfCtxFunc();
  const auto run_snprintf = [snprintf](char *destination, size_t size) {
    Libs::VaContext context{};
    context.reg_save_area.gp[0] = reinterpret_cast<uintptr_t>(destination);
    context.reg_save_area.gp[1] = size;
    context.reg_save_area.gp[2] = reinterpret_cast<uintptr_t>("%s:%d");
    context.reg_save_area.gp[3] = reinterpret_cast<uintptr_t>("hello");
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
