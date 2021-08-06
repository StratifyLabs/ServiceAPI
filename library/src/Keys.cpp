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
