#include <crypto.hpp>
#include <var.hpp>

#include "service/Keys.hpp"

using namespace service;

Keys::Keys(
  const crypto::DigitalSignatureAlgorithm &dsa,
  const crypto::Aes::Key &key)
  : DocumentAccess<Keys>("keys", "") {

  set_type("DigitalSignatureAlgorithm");
  set_iv(key.get_initialization_vector_string());
  set_public_key(dsa.key_pair().public_key().to_string());

  set_private_key_size(dsa.key_pair().private_key().size());
  set_status("active");

  // private key needs to be encrypted
  auto encrypted_private_key
    = Aes()
        .set_initialization_vector(key.initialization_vector())
        .set_key256(key.get_key256())
        .encrypt_cbc(
          View(Aes::get_padded_data(dsa.key_pair().private_key().data())));

  set_private_key(View(encrypted_private_key).to_string<GeneralString>());

  Aes::Key new_key;

  auto encrypted_key256
    = Aes()
        .set_initialization_vector(key.initialization_vector())
        .set_key256(key.get_key256())
        .encrypt_cbc(
          View(new_key.key256()));

  set_secure_key_256(View(encrypted_key256).to_string<GeneralString>());
}

bool Keys::is_password_required() const {
  return get_iv() != Aes::Key::get_null_iv_string();
}

crypto::DigitalSignatureAlgorithm
Keys::get_digital_signature_algorithm(const crypto::Aes::Key &key) const {

  auto key_pair
    = DigitalSignatureAlgorithm::KeyPair().set_public_key(get_dsa_public_key());

  // need to decrypt the private key
  const auto private_key_cipher = Data::from_string(get_private_key());

  const auto private_key_plain
    = Aes()
        .set_initialization_vector(key.initialization_vector())
        .set_key256(key.get_key256())
        .decrypt_cbc(private_key_cipher)
        .resize(get_private_key_size());

  key_pair.set_private_key(DigitalSignatureAlgorithm::PrivateKey(
    View(private_key_plain).to_string<GeneralString>()));

  return DigitalSignatureAlgorithm(key_pair);
}

crypto::Aes::Key256 Keys::get_key256(const crypto::Aes::Key &key) const {
  // need to decrypt the private key
  const auto key_cipher = Data::from_string(get_secure_key_256());

  if (key_cipher.size() != 32) {
    API_RETURN_VALUE_ASSIGN_ERROR(
      Aes::Key().nullify().key256(),
      "secure key 256 has bad length",
      EINVAL);
  }

  const auto key_plain
    = Aes()
        .set_initialization_vector(key.initialization_vector())
        .set_key256(key.get_key256())
        .decrypt_cbc(key_cipher);

  return Aes::Key(Aes::Key::Construct().set_key(
                    View(key_plain).to_string<GeneralString>()))
    .key256();
}
