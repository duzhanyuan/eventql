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
#pragma once
#include "eventql/eventql.h"
#include <eventql/util/stdtypes.h>
#include <eventql/util/autoref.h>
#include <eventql/util/SHA1.h>
#include <eventql/util/return_code.h>
#include <eventql/sql/svalue.h>
#include <eventql/sql/result_cursor.h>

namespace csql {

class TableExpression  {
public:

  virtual ~TableExpression() = default;

  virtual ReturnCode execute() = 0;
  virtual ReturnCode nextBatch(SVector* columns, size_t* len) = 0;

  virtual size_t getColumnCount() const = 0;
  virtual SType getColumnType(size_t idx) const = 0;

  virtual Option<SHA1Hash> getCacheKey() const {
    return None<SHA1Hash>();
  }

};

class SimpleTableExpression : public TableExpression {
public:
  using ColumnDefinition = std::pair<std::string, SType>;

  SimpleTableExpression(const std::vector<ColumnDefinition>& columns);

  ReturnCode nextBatch(SVector* columns, size_t* len) override;

  size_t getColumnCount() const override;
  SType getColumnType(size_t idx) const override;

protected:

  void addRow(const SValue* row);

private:
  std::vector<ColumnDefinition> columns_;
  std::vector<SVector> row_data_;
  size_t row_count_;
};

}
