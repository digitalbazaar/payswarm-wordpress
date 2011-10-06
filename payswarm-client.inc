<?php

/**
 * Decrypts an encrypted PaySwarm message.
 *
 * @param stdClass $encoded_message the message to decrypt.
 * @param string $pem the PEM-encoded private key to decrypt the message.
 *
 * @return string the decrypted message.
 */
function payswarm_decrypt_message($encoded_message, $pem)
{
   if($encoded_message->{'sec:algorithm'} !== 'rsa-aes-128-cbc')
   {
      $algorithm = $encoded_message->{'sec:algorithm'};
      throw new Exception('PaySwarm Decryption Exception:' .
         'Unknown encryption algorithm: ' . $algorithm);
   }

   // load private key from PEM
   $pkey = openssl_pkey_get_private($pem);
   if($pkey === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to load the private key in ');
   }

   // decrypt symmetric key
   $encryption_key = base64_decode($encoded_message->{'sec:encryptionKey'});
   if(openssl_private_decrypt($encryption_key, $skey, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the encryption key.');
   }

   // decrypt IV
   $encIv = base64_decode($encoded_message->{'sec:iv'});
   if(openssl_private_decrypt($encIv, $iv, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the initialization vector (IV).');
   }

   // free private key resource
   openssl_pkey_free($pkey);

   // decrypt data
   $data = openssl_decrypt(
      $encoded_message->{'sec:data'}, 'aes128', $skey, false, $iv);

   if(!$data)
   {
      print("skey: $skey\n");
      print("iv: $iv\n");
      print("encrypted data: " . base64_decode($encoded_message->{'sec:data'}) . '\n');

      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the encrypted message due to an incorrect ' .
         'symmetric key or an invalid initialization vector.');
   }

   // decode JSON message
   return json_decode($data);
}

// TODO: methods to use CURL to GET/POST
// TODO: method to fetch public key IRI and check for PA ownership based on
// PA IRI from config

?>
