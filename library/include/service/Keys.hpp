// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SERVICE_API_SERVICE_KEYS_HPP
#define SERVICE_API_SERVICE_KEYS_HPP

#include <chrono/ClockTimer.hpp>
#include <cloud/CloudObject.hpp>
#include <fs/LambdaFile.hpp>
#include <json/Json.hpp>
#include <var/String.hpp>
#include <crypto/Ecc.hpp>
#include <crypto/Aes.hpp>

#include "Document.hpp"

namespace service {

class Keys : public DocumentAccess<Keys> {
public:
  Keys() : DocumentAccess("keys", "") {}
  Keys(const var::StringView id) : DocumentAccess("keys", id) {}
  Keys(const crypto::DigitalSignatureAlgorithm & dsa, const crypto::Aes::Key & key);

  bool ping(const var::StringView id);

  json::JsonValue
  publish(const JsonValue &input, const chrono::MicroTime &timeout);

  crypto::DigitalSignatureAlgorithm get_digital_signature_algorithm(const crypto::Aes::Key & key) const;

  crypto::Dsa::PublicKey get_dsa_public_key() const {
    return crypto::Dsa::PublicKey(get_public_key());
  }

  crypto::Dsa::PrivateKey get_dsa_private_key() const {
    return crypto::Dsa::PrivateKey(get_private_key());
  }

  JSON_ACCESS_STRING(Keys, type);
  JSON_ACCESS_STRING(Keys, status);
  JSON_ACCESS_STRING(Keys, name);
  JSON_ACCESS_STRING(Keys, iv);
  JSON_ACCESS_INTEGER_WITH_KEY(Keys, privateKeySize, private_key_size);
  JSON_ACCESS_STRING_WITH_KEY(Keys, publicKey, public_key);
  JSON_ACCESS_STRING_WITH_KEY(Keys, privateKey, private_key);

private:
  API_AC(Keys, chrono::MicroTime, timeout);
  API_AB(Keys, stop, false);
};

} // namespace service

#endif // SERVICE_API_SERVICE_KEYS_HPP
