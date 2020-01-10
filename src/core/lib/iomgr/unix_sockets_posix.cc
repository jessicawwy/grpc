/*
 *
 * Copyright 2016 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <grpc/support/port_platform.h>

#include "src/core/lib/iomgr/port.h"

#ifdef GRPC_HAVE_UNIX_SOCKET

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include "absl/strings/str_cat.h"

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>

#include "src/core/lib/address_utils/parse_address.h"
#include "src/core/lib/gpr/useful.h"
#include "src/core/lib/iomgr/sockaddr.h"
#include "src/core/lib/iomgr/unix_sockets_posix.h"
#include "src/core/lib/transport/error_utils.h"

void grpc_create_socketpair_if_unix(int sv[2]) {
  GPR_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
}

absl::StatusOr<std::vector<grpc_resolved_address>>
grpc_resolve_unix_domain_address(absl::string_view name) {
  grpc_resolved_address addr;
  grpc_error_handle error = grpc_core::UnixSockaddrPopulate(name, &addr);
  if (error == GRPC_ERROR_NONE) {
    return std::vector<grpc_resolved_address>({addr});
  }
  auto result = grpc_error_to_absl_status(error);
  GRPC_ERROR_UNREF(error);
  return result;
}

absl::StatusOr<std::vector<grpc_resolved_address>>
grpc_resolve_unix_abstract_domain_address(const absl::string_view name) {
  grpc_resolved_address addr;
  grpc_error_handle error =
      grpc_core::UnixAbstractSockaddrPopulate(name, &addr);
  if (error == GRPC_ERROR_NONE) {
    return std::vector<grpc_resolved_address>({addr});
  }
  auto result = grpc_error_to_absl_status(error);
  GRPC_ERROR_UNREF(error);
  return result;
}

absl::StatusOr<std::vector<grpc_resolved_address>>
grpc_resolve_vsock_address(const absl::string_view name) {
  grpc_resolved_address addr;
  grpc_error_handle error = grpc_core::VsockaddrPopulate(name, &addr);
  if (error == GRPC_ERROR_NONE) {
    return std::vector<grpc_resolved_address>({addr});
  }
  auto result = grpc_error_to_absl_status(error);
  GRPC_ERROR_UNREF(error);
  return result;
}

int grpc_is_unix_socket(const grpc_resolved_address* resolved_addr) {
  const grpc_sockaddr* addr =
      reinterpret_cast<const grpc_sockaddr*>(resolved_addr->addr);
  return addr->sa_family == AF_UNIX;
}

int grpc_is_vsock(const grpc_resolved_address* resolved_addr) {
#ifdef GRPC_HAVE_LINUX_VSOCK
  const grpc_sockaddr* addr =
      reinterpret_cast<const grpc_sockaddr*>(resolved_addr->addr);
  return addr->sa_family == AF_VSOCK;
#else /* GRPC_HAVE_LINUX_VSOCK */
  return 0;
#endif /* GRPC_HAVE_LINUX_VSOCK */
}

void grpc_unlink_if_unix_domain_socket(
    const grpc_resolved_address* resolved_addr) {
  const grpc_sockaddr* addr =
      reinterpret_cast<const grpc_sockaddr*>(resolved_addr->addr);
  if (addr->sa_family != AF_UNIX) {
    return;
  }
  struct sockaddr_un* un = reinterpret_cast<struct sockaddr_un*>(
      const_cast<char*>(resolved_addr->addr));

  // There is nothing to unlink for an abstract unix socket
  if (un->sun_path[0] == '\0' && un->sun_path[1] != '\0') {
    return;
  }

  struct stat st;
  if (stat(un->sun_path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFSOCK) {
    unlink(un->sun_path);
  }
}

char* grpc_sockaddr_to_uri_vsock_if_possible(
    const grpc_resolved_address* resolved_addr) {
#ifdef GRPC_HAVE_LINUX_VSOCK
  const grpc_sockaddr* addr =
      reinterpret_cast<const grpc_sockaddr*>(resolved_addr->addr);

  if (addr->sa_family != AF_VSOCK) {
      return nullptr;
  }

  char *result;
  struct sockaddr_vm *vm = (struct sockaddr_vm*)addr;
  gpr_asprintf(&result, "vsock:%u:%u", vm->svm_cid, vm->svm_port);
  return result;
#else /* GRPC_HAVE_LINUX_VSOCK */
  return nullptr;
#endif /* GRPC_HAVE_LINUX_VSOCK */
}

#endif
