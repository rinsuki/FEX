#include <cstdint>
#include <unistd.h>

int main() {
  int fd_base = dup(STDOUT_FILENO);
  for (size_t i = 0; i < 15; ++i) {
    dup(fd_base);
  }

  // Specifically testing last as ~0U to ensure FEX doesn't hang
  constexpr uint32_t SYS_close_range = 436;
  ::syscall(SYS_close_range, fd_base + 1, ~0U, 0);

  // Ensure that fd_base itself wasn't closed in close_range
  int Result = close(fd_base);
  return Result;
}