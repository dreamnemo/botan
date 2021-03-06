/*
* Diffie-Hellman
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/internal/pk_utils.h>
#include <botan/dh.h>
#include <botan/workfactor.h>
#include <botan/pow_mod.h>
#include <botan/blinding.h>

namespace Botan {

/*
* DH_PublicKey Constructor
*/
DH_PublicKey::DH_PublicKey(const DL_Group& grp, const BigInt& y1)
   {
   m_group = grp;
   m_y = y1;
   }

/*
* Return the public value for key agreement
*/
std::vector<byte> DH_PublicKey::public_value() const
   {
   return unlock(BigInt::encode_1363(m_y, group_p().bytes()));
   }

/*
* Create a DH private key
*/
DH_PrivateKey::DH_PrivateKey(RandomNumberGenerator& rng,
                             const DL_Group& grp,
                             const BigInt& x_arg)
   {
   m_group = grp;
   m_x = x_arg;

   if(m_x == 0)
      {
      const BigInt& p = group_p();
      m_x.randomize(rng, dl_exponent_size(p.bits()));
      }

   if(m_y == 0)
      m_y = power_mod(group_g(), m_x, group_p());

   if(m_x == 0)
      gen_check(rng);
   else
      load_check(rng);
   }

/*
* Load a DH private key
*/
DH_PrivateKey::DH_PrivateKey(const AlgorithmIdentifier& alg_id,
                             const secure_vector<byte>& key_bits,
                             RandomNumberGenerator& rng) :
   DL_Scheme_PrivateKey(alg_id, key_bits, DL_Group::ANSI_X9_42)
   {
   if(m_y == 0)
      m_y = power_mod(group_g(), m_x, group_p());

   load_check(rng);
   }

/*
* Return the public value for key agreement
*/
std::vector<byte> DH_PrivateKey::public_value() const
   {
   return DH_PublicKey::public_value();
   }

namespace {

/**
* DH operation
*/
class DH_KA_Operation : public PK_Ops::Key_Agreement_with_KDF
   {
   public:
      typedef DH_PrivateKey Key_Type;
      DH_KA_Operation(const DH_PrivateKey& key, const std::string& kdf);

      secure_vector<byte> raw_agree(const byte w[], size_t w_len) override;
   private:
      const BigInt& m_p;

      Fixed_Exponent_Power_Mod m_powermod_x_p;
      Blinder m_blinder;
   };

DH_KA_Operation::DH_KA_Operation(const DH_PrivateKey& dh, const std::string& kdf) :
   PK_Ops::Key_Agreement_with_KDF(kdf),
   m_p(dh.group_p()),
   m_powermod_x_p(dh.get_x(), m_p),
   m_blinder(m_p,
             [](const BigInt& k) { return k; },
             [this](const BigInt& k) { return m_powermod_x_p(inverse_mod(k, m_p)); })
   {
   }

secure_vector<byte> DH_KA_Operation::raw_agree(const byte w[], size_t w_len)
   {
   BigInt input = BigInt::decode(w, w_len);

   if(input <= 1 || input >= m_p - 1)
      throw Invalid_Argument("DH agreement - invalid key provided");

   BigInt r = m_blinder.unblind(m_powermod_x_p(m_blinder.blind(input)));

   return BigInt::encode_1363(r, m_p.bytes());
   }

}

BOTAN_REGISTER_PK_KEY_AGREE_OP("DH", DH_KA_Operation);

}
