// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SERVICE_API_SERVICE_HARDWARE_HPP
#define SERVICE_API_SERVICE_HARDWARE_HPP

#include <sos/Sys.hpp>

#include "Document.hpp"

namespace service {

class Hardware : public DocumentAccess<Hardware> {
public:
  class Feature : public json::JsonValue {
  public:
    JSON_ACCESS_CONSTRUCT_OBJECT(Feature);
    JSON_ACCESS_STRING(Feature, key);
    JSON_ACCESS_STRING(Feature, value);
  };

  Hardware(const Id &id = Id());
  JSON_ACCESS_STRING_WITH_KEY(Hardware, imageUrl, image_url);
  JSON_ACCESS_STRING_WITH_KEY(Hardware, mbedDriveName, mbed_drive_name);
  JSON_ACCESS_STRING_WITH_KEY(Hardware, sessionTicket, session_ticket);
  JSON_ACCESS_ARRAY_WITH_KEY(Hardware, Feature, features, feature_list);

  using FeatureList = var::Vector<Feature>;

private:
};

} // namespace service

#endif // SERVICE_API_SERVICE_HARDWARE_HPP
