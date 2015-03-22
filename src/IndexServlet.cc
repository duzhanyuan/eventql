/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include "IndexServlet.h"

using namespace fnord;

namespace cm {

IndexServlet::IndexServlet(
    RefPtr<cm::IndexReader> index,
    RefPtr<fts::Analyzer> analyzer) :
    index_(index),
    analyzer_(analyzer) {}

void IndexServlet::handleHTTPRequest(
    fnord::http::HTTPRequest* req,
    fnord::http::HTTPResponse* res) {
  URI uri(req->uri());

  res->addHeader("Access-Control-Allow-Origin", "*");

  try {
    if (StringUtil::endsWith(uri.path(), "/search")) {
      return searchQuery(req, res, &uri);
    }

    res->setStatus(fnord::http::kStatusNotFound);
    res->addBody("not found");
  } catch (const Exception& e) {
    res->setStatus(http::kStatusInternalServerError);
    res->addBody(StringUtil::format("error: $0", e.getMessage()));
  }
}

void IndexServlet::searchQuery(
    http::HTTPRequest* req,
    http::HTTPResponse* res,
    URI* uri) {
  const auto& params = uri->queryParams();
  res->setStatus(fnord::http::kStatusOK);

  String search_query;
  if (!fnord::URI::getParam(params, "q", &search_query)) {
    res->addBody("error: missing ?q=... parameter");
    res->setStatus(http::kStatusBadRequest);
    return;
  }

  res->addBody("search!!! " + search_query);
}

}
