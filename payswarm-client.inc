<?php

/**
 * Decrypts an encrypted PaySwarm message.
 *
 * @param encMsg the message to decrypt.
 * @param pem the PEM-encoded private key to decrypt the message.
 *
 * @return the decrypted message.
 */
function payswarm_decrypt_message($encMsg, $pem)
{
   if($encMsg->{'sec:algorithm'} !== 'rsa-aes-128-cbc')
   {
      // FIXME: throw exception, unknown algorithm
   }

   // load private key from PEM
   $pkey = openssl_pkey_get_private($pem);
   if($pkey === false)
   {
      // FIXME: throw exception, could not load private key
   }

   // decrypt symmetric key
   $encKey = base64_decode($encMsg->{'sec:encryptionKey'});
   if(openssl_private_decrypt($encKey, $skey, $pkey) === false)
   {
      // FIXME: throw exception, could not decrypt IV
   }

   // decrypt IV
   $encIv = base64_decode($encMsg->{'sec:iv'});
   if(openssl_private_decrypt($encIv, $iv, $pkey) === false)
   {
      // FIXME: throw exception, could not decrypt IV
   }

   // free private key resource
   openssl_pkey_free($pkey);

   // decrypt data
   $data = openssl_decrypt($encMsg->{'sec:data'}, 'aes128', $skey, true, $iv);

   // decode JSON message
   return json_decode($data);
}

// TODO: methods to use CURL to GET/POST
// TODO: method to fetch public key IRI and check for PA ownership based on
// PA IRI from config

?>
