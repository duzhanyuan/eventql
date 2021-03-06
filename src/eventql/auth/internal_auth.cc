/**
 * Copyright (c) 2016 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#include "eventql/auth/internal_auth.h"
#include "eventql/util/cidr.h"
#include "eventql/db/database.h"
#include "eventql/config/process_config.h"

namespace eventql {

ReturnCode InternalAuth::authenticateInternal(
    Session* session,
    native_transport::NativeConnection* connection,
    const std::vector<std::pair<std::string, std::string>>& auth_data) {
  auto dbctx = session->getDatabaseContext();
  auto remote_host = connection->getRemoteHost();
  auto allowed_hosts = StringUtil::split(
      dbctx->config->getString("cluster.allowed_hosts").get(),
      ",");

  bool is_host_allowed = false;
  for (auto cidr_range : allowed_hosts) {
    // FIXME precompile patterns
    StringUtil::ltrim(&cidr_range);
    StringUtil::rtrim(&cidr_range);
    if (cidr_match(cidr_range, remote_host)) {
      is_host_allowed = true;
      break;
    }
  }

  if (is_host_allowed) {
    session->setIsInternal(true);
    return ReturnCode::success();
  } else {
    return ReturnCode::error(
        "EAUTH",
        "host is not allowed to connect (cluster.allowed_hosts)");
  }
}

} // namespace eventql

