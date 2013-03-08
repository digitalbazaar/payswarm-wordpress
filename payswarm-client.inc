<?php

/*
New BSD License (3-clause)
Copyright (c) 2010-2013, Digital Bazaar, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

Neither the name of Digital Bazaar, Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DIGITAL BAZAAR BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

require_once('jsonld.php');

/*

PAYSWARM CLIENT API
-------------------

The PaySwarm Client API allows vendors to register with a PaySwarm Authority,
sign listings for the items they wish to sell, and receive payments from
their customers.

INSTRUCTIONS
------------

First, implement all of the required hooks. Various hooks will be triggered
when making calls to the API. Most of the hooks involve providing the API with
a custom mechanism for doing HTTP GET/POST and storing/retrieving data from
a database. It is also highly recommended that the optional cache hooks be
implemented to prevent excessive network traffic when looking up PaySwarm
Authority configurations and public keys. To implement a hook, simply write
a function that takes the appropriate parameters and returns the appropriate
values. Then pass the hook name and the name of the custom function to
'payswarm_add_hook'. Look below for the specific hooks that must be
implemented.

Next, use the API to register as a vendor on the PaySwarm Authority of
choice, sign listings, and accept payments via customers' chosen PaySwarm
Authorities.

1. Add the PaySwarm Authorities that should be trusted by calling:

  payswarm_add_trusted_authority('trustedauthority:port');

  In this version of the API, any customer's PaySwarm Authority that the vendor
  would like to do business with must be manually added. The vendor's chosen
  PaySwarm Authority will be automatically added during the registration
  step.

2. Register as a vendor by calling:

  $url = payswarm_get_register_vendor_url(
    'myauthority:port',
    'http://myserver/myregistercallbackurl.php');

  The first parameter is the host and port of the PaySwarm Authority to
  register with. The second is a callback URL that will receive the result of
  the registration as POST data.

  Direct the vendor to the URL so that they can complete the registration
  process. Once the registration process is complete, the vendor's browser
  will POST the registration result to the callback URL provided.

3. On the callback page, get the POST value 'encrypted-message' and pass it
  to register the vendor:

  payswarm_register_vendor($_POST['encrypted-message']);

  To handle PHP magic quotes do this:

  if(get_magic_quotes_gpc()) {
    $encrypted_message = stripcslashes($_POST['encrypted-message']);
  }

  If no exceptions are thrown, registration is complete. The returned object
  is the PaySwarm Vendor's Preferences, including the Financial Account ID
  to use in Listings.

5. Create a JSON-LD PaySwarm Asset and Listing. When listing an Asset, its
  unique hash must be in the Listing. To generate an asset hash call:

  payswarm_hash($asset);

4. Sign a listing. Create a JSON-LD PaySwarm Listing and then sign it:

  payswarm_sign($listing);

  Display the listing information; the use of RDFa is recommended. Depending
  on the application's needs, it is sometimes a good idea (or a requirement)
  to regenerate signatures when the vendor's public key is changed.

  Note: A Listing also contains a License for the Asset. If the application
  knows the ID (IRI) of the License to use but not the License hash, and it
  does not have the necessary parser to obtain the License information from
  its ID, it may use the PaySwarm Authority's license service to cache and
  retrieve the License by its ID. Then payswarm_hash($license) can be called
  on the result to produce its hash.

5. When a customer indicates that they want to purchase the Asset in a
  Listing, call:

  $url = payswarm_get_purchase_url(
    'customersauthority:port',
    $listing_id,
    $listing_hash,
    'https://myserver/mypurchasecallbackurl.php');

  To get a URL to redirect the customer to their PaySwarm Authority to
  complete the purchase. The last parameter is a callback URL that will
  receive the result of the purchase as POST data.

  If the customer has previously completed a purchase and the response
  indicated that they set up a budget to handle automated purchases in the
  future, then an automated purchase can be attempted by calling:

  payswarm_purchase(
    'customersauthority:port',
    'https://customersauthority:port/i/customer',
    $listing_id,
    $listing_hash);

  In this version of the API, it is the responsibility of the application to
  determine the customer's PaySwarm Authority (usually by asking). A listing
  hash can be generated by calling:

  payswarm_hash($listing);

  To get the JSON-LD receipt from a purchase, call:

  payswarm_get_receipt($encrypted_message);

  Where $encrypted_message is either the result of a POST to the purchase
  callback or the result of the payswarm_purchase() call.

  The receipt will indicate the ID and hash of the Asset purchased as well
  as the ID and hash of the License for the Asset.

*/

class PaySwarmAsymmetricCipherException extends Exception {
};

// hook API
$payswarm_hooks = array();

// set jsonld context loader
jsonld_set_context_loader('payswarm_jsonld_load_context');

/**
 * Adds a hook. To add a hook, pass the name of the hook (eg: create_nonce) and
 * the user-defined function name to be called. Hooks are permitted to throw
 * exceptions as are any PaySwarm client API calls. API calls should be
 * wrapped in try/catch blocks as appropriate.
 *
 * Required protocol hooks:
 *
 * create_nonce(): Creates and stores a nonce that is to be given to a
 *   PaySwarm Authority so it can be returned in a signed and encrypted
 *   message.
 *
 * check_nonce($nonce): Checks a nonce previously created by create_nonce and
 *   removes it from storage. Returns true if the nonce is valid, false if not.
 *
 * get_jsonld($url): Returns the JSON-encoded body of an HTTP GET where
 *   the expected content-type is 'application/ld+json'.
 *
 * post_jsonld($url, $data): HTTP POSTs the given JSON-LD data to the given
 *   URL and returns the response body.
 *
 * Required storage hooks:
 *
 * get_public_key(): Returns the vendor's public key in PEM format.
 *
 * get_public_key_id(): Returns the ID (IRI) for the vendor's public key.
 *
 * get_private_key(): Returns the vendor's private key in PEM format.
 *
 * is_trusted_authority($id): Returns true if the given identity (IRI) is a
 *   trusted PaySwarm Authority, false if not.
 *
 * store_key_pair($public_pem, $private_pem): Stores the vendor's key pair.
 *
 * store_public_key_id($id): Stores the vendor's public key ID (IRI).
 *
 * store_trusted_authority($id): Stores the ID (IRI) of a trusted
 *   PaySwarm Authority.
 *
 * Optional cache hooks:
 *
 * cache_jsonld($id, $obj, $secs): Caches a JSON-LD object. The ID (IRI) for
 *   the object is given and the maxmimum number of seconds to cache.
 *
 * get_cached_jsonld($id): Gets a JSON-LD object from cache. Returns the
 *   object or false.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $hook the name of the hook.
 * @param string $func the name of the function to call.
 */
function payswarm_add_hook($hook, $func) {
  global $payswarm_hooks;
  $payswarm_hooks[$hook] = $func;
}

/**
 * Creates a default payswarm JSON-LD context.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return stdClass the default payswarm JSON-LD context.
 */
function payswarm_create_default_jsonld_context() {
  return (object)array(
    // aliases
    'id' => '@id',
    'type' => '@type',

    // prefixes
    'ccard' => 'https://w3id.org/commerce/creditcard#',
    'com' => 'https://w3id.org/commerce#',
    'dc' => 'http://purl.org/dc/terms/',
    'foaf' => 'http://xmlns.com/foaf/0.1/',
    'gr' => 'http://purl.org/goodrelations/v1#',
    'pto' => 'http://www.productontology.org/id/',
    'ps' => 'https://w3id.org/payswarm#',
    'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#',
    'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#',
    'sec' => 'https://w3id.org/security#',
    'vcard' => 'http://www.w3.org/2006/vcard/ns#',
    'xsd' => 'http://www.w3.org/2001/XMLSchema#',

    // general
    'address' => (object)array('@id' => 'vcard:adr', '@type' => '@id'),
    'comment' => 'rdfs:comment',
    'countryName' => 'vcard:country-name',
    'created' => (object)array(
      '@id' => 'dc:created', '@type' => 'xsd:dateTime'),
    'creator' => (object)array('@id' => 'dc:creator', '@type' => '@id'),
    'depiction' => (object)array('@id' => 'foaf:depiction', '@type' => '@id'),
    'description' => 'dc:description',
    'email' => 'foaf:mbox',
    'fullName' => 'vcard:fn',
    'label' => 'rdfs:label',
    'locality' => 'vcard:locality',
    'postalCode' => 'vcard:postal-code',
    'region' => 'vcard:region',
    'streetAddress' => 'vcard:street-address',
    'title' => 'dc:title',
    'website' => (object)array('@id' => 'foaf:homepage', '@type' => '@id'),
    'Address' => 'vcard:Address',

    // bank
    'bankAccount' => 'bank:account',
    'bankAccountType' => (object)array(
      '@id' => 'bank:accountType', '@type' => '@vocab'),
    'bankRoutingNumber' => 'bank:routing',
    'BankAccount' => 'bank:BankAccount',
    'Checking' => 'bank:Checking',
    'Savings' => 'bank:Savings',

    // credit card
    'cardBrand' => (object)array('@id' => 'ccard:brand', '@type' => '@vocab'),
    'cardCvm' => 'ccard:cvm',
    'cardExpMonth' => (object)array(
      '@id' => 'ccard:expMonth', '@type' => 'xsd:integer'),
    'cardExpYear' => (object)array(
      '@id' => 'ccard:expYear', '@type' => 'xsd:integer'),
    'cardNumber' => 'ccard:number',
    'AmericanExpress' => 'ccard:AmericanExpress',
    'ChinaUnionPay' => 'ccard:ChinaUnionPay',
    'CreditCard' => 'ccard:CreditCard',
    'Discover' => 'ccard:Discover',
    'Visa' => 'ccard:Visa',
    'MasterCard' => 'ccard:MasterCard',

    // commerce
    'account' => (object)array('@id' => 'com:account', '@type' => '@id'),
    'amount' => 'com:amount',
    'authorized' => (object)array(
      '@id' => 'com:authorized', '@type' => 'xsd:dateTime'),
    'balance' => 'com:balance',
    'currency' => (object)array('@id' => 'com:currency', '@type' => '@vocab'),
    'destination' => (object)array(
      '@id' => 'com:destination', '@type' => '@id'),
    'maximumAmount' => 'com:maximumAmount',
    'maximumPayeeRate' => 'com:maximumPayeeRate',
    'minimumAmount' => 'com:minimumAmount',
    'payee' => (object)array(
      '@id' => 'com:payee', '@type' => '@id', '@container' => '@set'),
    'payeeApplyAfter' => (object)array(
      '@id' => 'com:payeeApplyAfter', '@container' => '@set'),
    'payeeApplyGroup' => (object)array(
      '@id' => 'com:payeeApplyGroup', '@container' => '@set'),
    'payeeApplyType' => (object)array(
      '@id' => 'com:payeeApplyType', '@type' => '@vocab'),
    'payeeExemptGroup' => (object)array(
      '@id' => 'com:payeeExemptGroup', '@container' => '@set'),
    'payeeGroup' => (object)array(
      '@id' => 'com:payeeGroup', '@container' => '@set'),
    'payeeGroupPrefix' => (object)array(
      '@id' => 'com:payeeGroupPrefix', '@container' => '@set'),
    'payeeLimitation' => (object)array(
      '@id' => 'com:payeeLimitation', '@type' => '@vocab'),
    'payeeRate' => 'com:payeeRate',
    'payeeRateType' => (object)array(
      '@id' => 'com:payeeRateType', '@type' => '@vocab'),
    'payeeRule' => (object)array(
      '@id' => 'com:payeeRule', '@type' => '@id', '@container' => '@set'),
    'paymentGateway' => 'com:paymentGateway',
    'paymentMethod' => (object)array(
      '@id' => 'com:paymentMethod', '@type' => '@vocab'),
    'paymentToken' => 'com:paymentToken',
    'referenceId' => 'com:referenceId',
    'settled' => (object)array(
      '@id' => 'com:settled', '@type' => 'xsd:dateTime'),
    'source' => (object)array('@id' => 'com:source', '@type' => '@id'),
    'transfer' => (object)array(
      '@id' => 'com:transfer', '@type' => '@id', '@container' => '@set'),
    'vendor' => (object)array('@id' => 'com:vendor', '@type' => '@id'),
    'voided' => (object)array('@id' => 'com:voided', '@type' => 'xsd:dateTime'),
    'ApplyExclusively' => 'com:ApplyExclusively',
    'ApplyInclusively' => 'com:ApplyInclusively',
    'FinancialAccount' => 'com:Account',
    'FlatAmount' => 'com:FlatAmount',
    'Deposit' => 'com:Deposit',
    'NoAdditionalPayeesLimitation' => 'com:NoAdditionalPayeesLimitation',
    'Payee' => 'com:Payee',
    'PayeeRule' => 'com:PayeeRule',
    'PayeeScheme' => 'com:PayeeScheme',
    'PaymentToken' => 'com:PaymentToken',
    'Percentage' => 'com:Percentage',
    'Transaction' => 'com:Transaction',
    'Transfer' => 'com:Transfer',
    'Withdrawal' => 'com:Withdrawal',

    // currencies
    'USD' => 'https://w3id.org/currencies/USD',

    // error
    // FIXME
    // 'errorMessage' => 'err:message'

    // payswarm
    'asset' =>(object)array('@id' => 'ps:asset', '@type' => '@id'),
    'assetAcquirer' => (object)array(
      '@id' => 'ps:assetAcquirer', '@type' => '@id'),
    // FIXME: support inline content
    'assetContent' => (object)array(
      '@id' => 'ps:assetContent', '@type' => '@id'),
    'assetHash' => 'ps:assetHash',
    'assetProvider' => (object)array(
      '@id' => 'ps:assetProvider', '@type' => '@id'),
    'authority' => (object)array('@id' => 'ps:authority', '@type' => '@id'),
    'contract' => (object)array('@id' => 'ps:contract', '@type' => '@id'),
    'identityHash' => 'ps:identityHash',
    // FIXME: move?
    'ipv4Address' => 'ps:ipv4Address',
    'license' => (object)array('@id' => 'ps:license', '@type' => '@id'),
    'licenseHash' => 'ps:licenseHash',
    'licenseTemplate' => 'ps:licenseTemplate',
    'licenseTerms' => (object)array(
      '@id' => 'ps:licenseTerms', '@type' => '@id'),
    'listing' => (object)array('@id' => 'ps:listing', '@type' => '@id'),
    'listingHash' => 'ps:listingHash',
    'listingRestrictions' => (object)array(
      '@id' => 'ps:listingRestrictions', '@type' => '@id'),
    'preferences' => (object)array(
      '@id' => 'ps:preferences', '@type' => '@vocab'),
    'validFrom' => (object)array(
      '@id' => 'ps:validFrom', '@type' => 'xsd:dateTime'),
    'validUntil' => (object)array(
      '@id' => 'ps:validUntil', '@type' => 'xsd:dateTime'),
    'Asset' => 'ps:Asset',
    'Budget' => 'ps:Budget',
    'Contract' => 'ps:Contract',
    'License' => 'ps:License',
    'Listing' => 'ps:Listing',
    'PersonalIdentity' => 'ps:PersonalIdentity',
    'IdentityPreferences' => 'ps:IdentityPreferences',
    'Profile' => 'ps:Profile',
    'PurchaseRequest' => 'ps:PurchaseRequest',
    'PreAuthorization' => 'ps:PreAuthorization',
    'Receipt' => 'ps:Receipt',
    'VendorIdentity' => 'ps:VendorIdentity',

    // security
    'cipherAlgorithm' => 'sec:cipherAlgorithm',
    'cipherData' => 'sec:cipherData',
    'cipherKey' => 'sec:cipherKey',
    'digestAlgorithm' => 'sec:digestAlgorithm',
    'digestValue' => 'sec:digestValue',
    'expiration' => (object)array(
      '@id' => 'sec:expiration', '@type' => 'xsd:dateTime'),
    'initializationVector' => 'sec:initializationVector',
    'nonce' => 'sec:nonce',
    'normalizationAlgorithm' => 'sec:normalizationAlgorithm',
    'owner' => (object)array('@id' => 'sec:owner', '@type' => '@id'),
    'password' => 'sec:password',
    'privateKey' => (object)array('@id' => 'sec:privateKey', '@type' => '@id'),
    'privateKeyPem' => 'sec:privateKeyPem',
    'publicKey' => (object)array('@id' => 'sec:publicKey', '@type' => '@id'),
    'publicKeyPem' => 'sec:publicKeyPem',
    'publicKeyService' => (object)array(
      '@id' => 'sec:publicKeyService', '@type' => '@id'),
    'revoked' => (object)array(
      '@id' => 'sec:revoked', '@type' => 'xsd:dateTime'),
    'signature' => 'sec:signature',
    'signatureAlgorithm' => 'sec:signatureAlgorithm',
    'signatureValue' => 'sec:signatureValue',
    'EncryptedMessage' => 'sec:EncryptedMessage',
    'CryptographicKey' => 'sec:Key',
    'GraphSignature2012' => 'sec:GraphSignature2012'
  );
}

/**
 * JSON-encodes (with unescaped slashes) the given stdClass or array.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $input the native PHP stdClass or array which will be
 *          converted to JSON by json_encode().
 *
 * @return the encoded JSON data.
 */
function payswarm_jsonld_encode($input) {
  // newer PHP has a flag to avoid escaped '/'
  if(defined('JSON_UNESCAPED_SLASHES')) {
     $json = json_encode($input, JSON_UNESCAPED_SLASHES);
  }
  else {
     // use a simple string replacement of '\/' to '/'.
     $json = str_replace('\\/', '/', json_encode($input));
  }

  return $json;
}

/**
 * Decodes a serialized JSON-LD object.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $input the JSON-LD input.
 *
 * @return mixed the resolved JSON-LD object, null on error.
 */
function payswarm_jsonld_decode($input) {
  // JSON-decode
  return json_decode($input);
}

/**
 * Retrieves a JSON-LD object over HTTP.
 *
 * @param string $url the URL to HTTP GET.
 * @param boolean $cache true to cache the response.
 *
 * @return stdClass the JSON-LD object.
 */
function payswarm_get_jsonld($url, $cache) {
  global $payswarm_hooks;

  // use cache if available
  $rval = payswarm_get_cached_jsonld($url);
  if($rval === false) {
    // retrieve response
    $rval = call_user_func($payswarm_hooks['get_jsonld'], $url);
    $rval = payswarm_jsonld_decode($rval);
    if($rval === null) {
      throw new Exception("Invalid response from '$url': Malformed JSON.");
    }

    // cache response
    if($cache) {
      payswarm_cache_jsonld($url, $rval);
    }
  }

  return $rval;
}

/**
 * HTTP POSTs a JSON-LD object.
 *
 * @param string $url the URL to HTTP POST to.
 * @param stdClass $obj the JSON-LD object.
 *
 * @return stdClass the JSON-LD response.
 */
function payswarm_post_jsonld($url, $obj) {
  global $payswarm_hooks;

  try {
    $data = payswarm_jsonld_encode($obj);
    $rval = call_user_func($payswarm_hooks['post_jsonld'], $data, $url);
  }
  catch(Exception $e) {
    throw new Exception('PaySwarm Exception: ' .
      "Error while trying to POST to '$url': " . $e->getMessage());
  }

  // decode response
  $rval = payswarm_jsonld_decode($rval);
  if($rval === null) {
    throw new Exception("Invalid response from '$url': Malformed JSON.");
  }
  return $rval;
}

/**
 * Caches a JSON-LD object if a cache is available.
 *
 * @param string $id the ID of the JSON-LD object.
 * @param stdClass $obj the JSON-LD object to cache.
 */
function payswarm_cache_jsonld($id, $obj) {
  global $payswarm_hooks;
  if(isset($payswarm_hooks['cache_jsonld'])) {
    call_user_func($payswarm_hooks['cache_jsonld'], $id, $obj, 60*5);
  }
}

/**
 * Gets a cached JSON-LD object if available.
 *
 * @param string $id the ID of the JSON-LD object.
 *
 * @return stdClass the cached JSON-LD object or false.
 */
function payswarm_get_cached_jsonld($id) {
  global $payswarm_hooks;
  $rval = false;
  if(isset($payswarm_hooks['get_cached_jsonld'])) {
    $rval = call_user_func($payswarm_hooks['get_cached_jsonld'], $id);
  }
  return $rval;
}

/**
 * Retrieves a JSON-LD context URL, returning the context.
 *
 * @param string $url the URL for the JSON-LD context.
 *
 * @return stdClass the JSON-LD context.
 */
function payswarm_jsonld_load_context($url) {
  // FIXME: hack until https://w3id.org/payswarm/v1 is ready
  if($url === 'https://w3id.org/payswarm/v1') {
    return (object)array(
      '@context' => payswarm_create_default_jsonld_context());
  }

  // use cache if available
  $rval = payswarm_get_cached_jsonld($url);
  if($rval === false) {
    // do secure retrieval of URL
    $rval = jsonld_default_get_secure_url($url);
    if($cache) {
      payswarm_cache_jsonld($url, $rval);
    }
  }

  return $rval;
}

/**
 * Check if a JSON-LD object has a specific type.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj the JSON-LD object.
 * @param string $type the type to check for.
 *
 * @return true if type found, false if not.
 */
function payswarm_jsonld_has_type($obj, $type) {
  $rval = false;

  if(property_exists($obj, 'type')) {
    $types = is_array($obj->type) ? $obj->type : array($obj->type);
    $length = count($types);
    for($t = 0; $t < $length && !$rval; ++$t) {
      $rval = ($types[$t] == $type);
    }
  }

  return $rval;
}

/**
 * Gets a remote public key.
 *
 * @param string $id the ID for the public key.
 *
 * @return stdClass the public key.
 */
function payswarm_get_public_key($id) {
  // retrieve public key
  $key = payswarm_get_jsonld($id, false);
  if(!property_exists($key, 'publicKeyPem')) {
    throw new Exception('PaySwarm Security Exception: ' .
      'Could not get public key. Unknown format.');
  }

  // cache public key
  payswarm_cache_jsonld($id, $key);

  return $key;
}

/**
 * Creates a nonce for a secure message.
 *
 * @return string the nonce.
 */
function payswarm_create_nonce() {
  global $payswarm_hooks;
  return call_user_func($payswarm_hooks['create_nonce']);
}

/**
 * Checks the nonce from a secure message.
 *
 * @param string $nonce the nonce.
 *
 * @return true if the nonce is valid, false if not.
 */
function payswarm_check_nonce($nonce) {
  global $payswarm_hooks;
  return call_user_func($payswarm_hooks['check_nonce'], $nonce);
}

/**
 * Generates a hash of the JSON-LD encoded data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj the JSON-LD object to hash.
 *
 * @return the SHA-1 hash of the encoded JSON data.
 */
function payswarm_hash($obj) {
  // SHA-1 hash JSON
  $options = array('format' => 'application/nquads');
  return 'urn:sha256:' . hash('sha256', jsonld_normalize($obj, $options));
}

/**
 * Signs a JSON-LD object, adding a signature field to it. If a signature
 * date is not provided then the current date will be used.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj the JSON-LD object to sign.
 * @param string $date the ISO8601-formatted signature creation date (optional).
 */
function payswarm_sign($obj, $date=null, $nonce='') {
  global $payswarm_hooks;

  if(!$date) {
    $date = date_format(
      date_create('now', new DateTimeZone('UTC')), DateTime::W3C);
  }

  // get private key pem and public key ID
  $pem = call_user_func($payswarm_hooks['get_private_key']);
  $public_key_id = call_user_func($payswarm_hooks['get_public_key_id']);

  // generate base64-encoded signature
  $options = array('format' => 'application/nquads');
  $nquads = jsonld_normalize($obj, $options);
  payswarm_raw_sign($nonce . $date . $nquads, $signature, $pem);
  $signature = base64_encode($signature);

  // add signature to object
  $obj->signature = (object)array(
    'type' => 'GraphSignature2012',
    'created' => $date,
    'creator' => $public_key_id,
    'signatureValue' => $signature
  );
  // add optional nonce
  if($nonce) {
    $obj->signature->nonce = $nonce;
  }
}

/**
 * Verifies a JSON-LD digitally signed object.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj the JSON-LD object to verify.
 *
 * @return true if verified, false if not (exception thrown).
 */
function payswarm_verify($obj) {
  global $payswarm_hooks;
  $rval = false;

  // frame message to retrieve signature
  $frame = (object)array(
    '@context' => payswarm_create_default_jsonld_context(),
    'signature' => (object)array(
      'created' => new stdClass(),
      'creator' => new stdClass(),
      'signatureValue' => new stdClass(),
      'nonce' => new stdClass()
    )
  );
  $obj = jsonld_frame($obj, $frame);
  if(count($obj->{'@graph'}) === 0 ||
    $obj->{'@graph'}[0]->signature === null) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The message is not digitally signed.');
  }

  // save signature property and remove from object
  $result = $obj->{'@graph'}[0];
  $sprop = $result->signature;
  unset($result->signature);

  // check the message nonce
  if(property_exists($sprop, 'nonce')) {
    $valid_nonce = payswarm_check_nonce($sprop->nonce);
    if(!$valid_nonce) {
      throw new Exception('PaySwarm Security Exception: ' .
        'The message nonce is invalid.');
    }
  }

  // ensure signature timestamp is +/- 15 minutes
  $now = time();
  $time = date_create($sprop->created)->getTimestamp();
  if($time < ($now - 15*60) || $time > ($now + 15*60)) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The message digital signature timestamp is out of range.');
  }

  // fetch the public key for the signature
  $key = payswarm_get_public_key($sprop->creator);
  $pem = $key->publicKeyPem;

  // ensure key has not been revoked
  if(property_exists($key, 'revoked')) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The message was signed with a key that has been revoked.');
  }

  // verify key owner
  $trusted = call_user_func(
    $payswarm_hooks['is_trusted_authority'], $key->owner);
  if(!$trusted) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The message is not signed by a trusted public key.');
  }

  // normalize and serialize the object
  $options = array('format' => 'application/nquads');
  $nquads = jsonld_normalize($obj, $options);

  // get the data to hash
  $data = $sprop->nonce . $sprop->created . $nquads;

  // decode the signature value
  $sig = base64_decode($sprop->signatureValue);

  // verify the signature
  $rc = payswarm_raw_verify($data, $sig, $pem);
  if($rc === 1) {
    $rval = true;
  }
  else if($rc === -1) {
    // throw exception, error while trying to verify
    throw new Exception('PaySwarm Security Exception: ' .
      'Low-level API error: ' . openssl_error_string());
  }
  else {
    throw new Exception('PaySwarm Security Exception: ' .
      'The digital signature on the message is invalid.');
  }

  return $rval;
}

/**
 * Decrypts an encrypted JSON-LD object.
 *
 * @param stdClass $encrypted the message to decrypt.
 * @param string $pem the PEM-encoded private key to decrypt the message.
 *
 * @return stdClass the decrypted JSON-LD object.
 */
function payswarm_decrypt($encrypted) {
  global $payswarm_hooks;

  if(!property_exists($encrypted, 'cipherAlgorithm') ||
    $encrypted->cipherAlgorithm !== 'rsa-sha256-aes-128-cbc') {
    $algorithm = $encrypted->cipherAlgorithm;
    throw new Exception('PaySwarm Security Exception: ' .
      'Unknown encryption algorithm "' . $algorithm . '"');
  }

  // get private key pem
  $pem = call_user_func($payswarm_hooks['get_private_key']);

  // load private key from PEM
  $pkey = openssl_pkey_get_private($pem);
  if($pkey === false) {
    throw new Exception('PaySwarm Security Exception: ' .
      'Failed to load the private key.');
  }

  // decrypt symmetric key (be lenient with padding)
  $encryption_key = base64_decode($encrypted->cipherKey);
  if(openssl_private_decrypt(
    $encryption_key, $skey, $pkey, OPENSSL_PKCS1_OAEP_PADDING) === false) {
    if(openssl_private_decrypt($encryption_key, $skey, $pkey) === false) {
      throw new PaySwarmAsymmetricCipherException(
        'PaySwarm Security Exception: ' .
        'Failed to decrypt the encryption key.');
    }
  }

  // decrypt IV
  $enc_iv = base64_decode($encrypted->initializationVector);
  if(openssl_private_decrypt(
    $enc_iv, $iv, $pkey, OPENSSL_PKCS1_OAEP_PADDING) === false) {
    if(openssl_private_decrypt($enc_iv, $iv, $pkey) === false) {
      throw new Exception('PaySwarm Security Exception: ' .
        'Failed to decrypt the initialization vector (IV).');
    }
  }

  // free private key resource
  openssl_pkey_free($pkey);

  // decrypt data
  $data = openssl_decrypt($encrypted->cipherData, 'aes128', $skey, false, $iv);
  if($data === false) {
    throw new Exception('PaySwarm Security Exception: ' .
      'Failed to decrypt the encrypted message due to an incorrect ' .
      'symmetric key or an invalid initialization vector (IV).');
  }

  // decode JSON message
  return payswarm_jsonld_decode($data);
}

/**
 * Decodes a JSON-encoded, encrypted, digitally-signed message from a
 * PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $json_message the json-encoded message to verify.
 *
 * @return the JSON-LD decoded message (exception thrown on error).
 */
function payswarm_decode_authority_message($json_message) {
  // convert message from json
  $msg = payswarm_jsonld_decode($json_message);
  if($msg === NULL) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The message contains malformed JSON.');
  }

  // decrypt and verify message
  $rval = payswarm_decrypt($msg);
  payswarm_verify($rval);
  return $rval;
}

/**
 * Gets the config for a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $host the PaySwarm Authority host and port.
 *
 * @return stdClass the PaySwarm Authority config.
 */
function payswarm_get_authority_config($host) {
  // get config
  $url = "https://$host/.well-known/payswarm";
  $config = payswarm_get_jsonld($url, true);

  // TODO: validate config

  return $config;
}

/**
 * Caches a license at the PaySwarm Authority and returns the result.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $id the ID of the license to cache.
 *
 * @return stdClass the license.
 */
function payswarm_authority_cache_license($host, $id) {
  // get license service
  $config = payswarm_get_authority_config($host);
  $url = $config->licenseService;

  // sign message
  $msg = new stdClass();
  $msg->{'@context'} = payswarm_create_default_jsonld_context();
  $msg->license = $id;
  payswarm_sign($msg);

  // post message
  $rval = payswarm_post_jsonld($url, $msg);

  // check license
  if(!$rval || !is_object($rval)) {
    throw new Exception('PaySwarm Exception: ' .
      'Invalid response when caching license.');
  }
  // FIXME: use JSON-LD exceptions
  else if(is_object($rval) && property_exists($rval, 'message')) {
    throw new Exception('PaySwarm Exception: ' .
      'Error while caching license: ' .
      $rval->message . ', ' . $rval->type);
  }

  return $rval;
}

/**
 * Generates a PEM-encoded key pair and stores it by calling the
 * 'store_key_pair' hook.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return array an array with the key pair as 'public_key' and 'private_key'.
 */
function payswarm_create_key_pair() {
  global $payswarm_hooks;

  // generate the key pair
  $config = array('private_key_bits' => 2048);
  $key_pair = openssl_pkey_new($config);

  // get private key and public key in PEM format
  openssl_pkey_export($key_pair, $private_key);
  $public_key = openssl_pkey_get_details($key_pair);
  $public_key = $public_key['key'];

  // free the key pair
  openssl_free_key($key_pair);

  // store key pair
  call_user_func($payswarm_hooks['store_key_pair'], $public_key, $private_key);

  return array('public_key' => $public_key, 'private_key' => $private_key);
}

/**
 * Adds a trusted PaySwarm Authority. Only trusted PaySwarm Authorities can
 * be used in financial transactions.
 *
 * @param string $host the PaySwarm Authority host and port.
 */
function payswarm_add_trusted_authority($host) {
  global $payswarm_hooks;

  // get authority config
  $config = payswarm_get_authority_config($host);

  // store authority identity
  $id = $config->authorityIdentity;
  call_user_func($payswarm_hooks['store_trusted_authority'], $id);
}

/**
 * Get the PaySwarm Authority's vendor registration URL, including the
 * parameters required to register the vendor. If a key pair does not exist
 * it will be generated, otherwise the existing key pair will be used unless
 * $overwrite_key_pair is set to true.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $callback the callback URL for the registration result.
 * @param boolean $overwrite_key_pair true to generate a new key-pair even if
 *          there is an existing one.
 *
 * @return string the URL for registering the vendor.
 */
function payswarm_get_register_vendor_url(
  $host, $callback, $overwrite_key_pair=false) {
  global $payswarm_hooks;

  // automatically trust given payswarm authority
  payswarm_add_trusted_authority($host);

  // get register URL from authority config
  $config = payswarm_get_authority_config($host);
  $register_url = $config->vendorRegistrationService;

  // use existing public key if overwrite is not specified
  $public_key = false;
  if(!$overwrite_key_pair) {
    $public_key = call_user_func($payswarm_hooks['get_public_key']);
  }

  // no public key available (or overwriting), generate new key pair
  if($public_key === false) {
    $key_pair = payswarm_create_key_pair();
    $public_key = $key_pair['public_key'];
  }

  // add query parameters to the register URL
  $register_url = payswarm_add_query_vars(
    $register_url, array(
    'public-key' => $public_key,
    'registration-callback' => $callback,
    'response-nonce' => payswarm_create_nonce())
  );

  return $register_url;
}

/**
 * Completes the vendor registration process by verifying the response
 * from the PaySwarm Authority.
 *
 * @param string $msg the JSON-encoded encrypted registration response message.
 *
 * @return stdClass the PaySwarm Vendor Preferences.
 */
function payswarm_register_vendor($msg) {
  global $payswarm_hooks;

  // decode message
  $prefs = payswarm_decode_authority_message($msg);

  // check message type
  if(payswarm_jsonld_has_type($prefs, 'Error')) {
    throw new Exception('PaySwarm Registration Exception: ' .
      $msg->errorMessage);
  }
  else if(!payswarm_jsonld_has_type($prefs, 'IdentityPreferences')) {
    throw new Exception('PaySwarm Registration Exception: ' .
      'Invalid registration response from PaySwarm Authority.');
  }

  // store public key ID
  call_user_func($payswarm_hooks['store_public_key_id'], $prefs->publicKey);

  return $prefs;
}

/**
 * Get the PaySwarm Authority's purchase URL, including the parameters
 * identifying the Listing with the Asset to be purchased.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $listing_id the ID (IRI) for the Listing.
 * @param string $listing_hash the hash for the Listing.
 * @param string $callback the callback URL for the purchase result.
 *
 * @return string the URL for purchasing.
 */
function payswarm_get_purchase_url(
  $host, $listing_id, $listing_hash, $callback) {

  // get purchase URL from authority config
  $config = payswarm_get_authority_config($host);
  $purchase_url = $config->paymentService;

  // add query parameters to the purchase URL
  $purchase_url = payswarm_add_query_vars(
    $purchase_url, array(
    'listing' => $listing_id,
    'listing-hash' => $listing_hash,
    'callback' => $callback,
    'response-nonce' => payswarm_create_nonce())
  );

  return $purchase_url;
}

/**
 * Performs an automated purchase on behalf of a customer who has previously
 * authorized it.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $id the ID (IRI) of the customer.
 * @param string $listing_id the ID (IRI) for the Listing.
 * @param string $listing_hash the hash for the Listing.
 *
 * @return string the JSON-encoded encrypted purchase response message.
 */
function payswarm_purchase($host, $id, $listing_id, $listing_hash) {
  // TODO: implement
}

/**
 * Completes the purchase process by verifying the response from the PaySwarm
 * Authority and returning the receipt.
 *
 * @param string $msg the JSON-encoded encrypted purchase response message.
 *
 * @return stdClass the PaySwarm Receipt.
 */
function payswarm_get_receipt($msg) {
  // decode message
  $receipt = payswarm_decode_authority_message($msg);

  // check message type
  if(payswarm_jsonld_has_type($receipt, 'Error')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      $receipt->errorMessage);
  }
  else if(!payswarm_jsonld_has_type($receipt, 'Receipt')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Invalid purchase response from PaySwarm Authority.');
  }

  // validate receipt
  if(!property_exists($receipt, 'contract') or !is_object($receipt->contract)) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Unknown Receipt format.');
  }

  // validate contract
  $contract = $receipt->contract;
  if(!property_exists($contract, 'assetAcquirer') or
    !property_exists($contract, 'asset') or
    !property_exists($contract, 'license')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Unknown Contract format.');
  }

  return $receipt;
}

/**
 * Add query variables to an existing url.
 *
 * @param string $url the url to add the query vars to.
 * @param array $qvars the query variables to add, eg: array('foo' => 'bar').
 *
 * @return string the updated url.
 */
function payswarm_add_query_vars($url, $qvars) {
  $parsed = parse_url($url);
  if(isset($parsed['query'])) {
    parse_str($parsed['query'], $query);
    $query = array_merge($query, $qvars);
  }
  else {
    $query = $qvars;
  }
  $scheme = isset($parsed['scheme']) ? $parsed['scheme'] . '://' : '';
  $host = isset($parsed['host']) ? $parsed['host'] : '';
  $port = isset($parsed['port']) ? ':' . $parsed['port'] : '';
  $path = isset($parsed['path']) ? $parsed['path'] : '';
  $query = (count($query) > 0) ? '?' . http_build_query($query) : '';
  $fragment = isset($parsed['fragment']) ? $parsed['fragment'] : '';
  return "$scheme$host$port$path$query$fragment";
}

/**
 * Get query variables from a url.
 *
 * @param string $url the url to add the query vars to.
 *
 * @return array an array of the query vars.
 */
function payswarm_get_query_vars($url) {
  parse_str(parse_url($url, PHP_URL_QUERY), $query);
  return $query;
}

/**
 * Determines whether or not the given Listing's validity period has passed.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $listing the Listing to check.
 *
 * @return boolean true if the validity period still applies, false if not.
 */
function payswarm_is_listing_valid($listing) {
  if(!$listing) {
    return false;
  }
  $now = date_create();
  $valid_from = date_create($listing->validFrom);
  $valid_until = date_create($listing->validUntil);
  return ($now >= $valid_from && $now <= $valid_until);
}

/**
 * Default POST JSON-LD hook.
 *
 * @param string $url the URL.
 * @param string $data the JSON-LD data.
 *
 * @return string the retrieved JSON-LD.
 */
function payswarm_default_post_jsonld($url, $data) {
  $opts = array('http' =>
    array(
      'method' => "POST",
      'header' =>
        "Accept: application/ld+json\r\n" .
        "Content-Type: application/ld+json\r\n" .
        "Content-Length: " . strlen($data) . "\r\n" .
        "User-Agent: PaySwarm PHP Client/1.0\r\n",
      'content' => $data));
  $stream = stream_context_create($opts);
  $result = @file_get_contents($url, false, $stream);
  if($result === false) {
    throw new Exception("Could not POST to url: '$url'");
  }
  return $result;
}

/** Default GET/POST JSON-LD hooks. */
payswarm_add_hook('get_jsonld', 'jsonld_default_get_url');
payswarm_add_hook('post_jsonld', 'payswarm_default_post_jsonld');

/**
 * A compatibility wrapper for producing a digital signature.
 *
 * @param string $data the data to sign.
 * @param string $signature the signature variable to be set.
 * @param string $pem the private key in PEM format.
 *
 * @return boolean true on success, false on failure.
 */
function payswarm_raw_sign($data, &$signature, $pem) {
  if(defined('OPENSSL_ALGO_SHA256')) {
    return openssl_sign($data, $signature, $pem, OPENSSL_ALGO_SHA256);
  }

  // load private key from PEM
  $pkey = openssl_pkey_get_private($pem);
  if($pkey === false) {
    throw new Exception('PaySwarm Security Exception: ' .
      'Failed to load the private key.');
  }

  // manually do pkcs1 v1.5 encoding (DigestInfo)
  $hash = hash('sha256', $data);
  $digestInfo = pack('H*', '3031300d060960864801650304020105000420' . $hash);

  $rval = openssl_private_encrypt(
    $digestInfo, $signature, $pkey, OPENSSL_PKCS1_PADDING);

  // free private key resource
  openssl_pkey_free($pkey);

  return $rval;
}

/**
 * A compatibility wrapper for verifying a digital signature.
 *
 * @param string $data the data to verify.
 * @param string $signature the signature to verify.
 * @param string $pem the public key in PEM format.
 *
 * @return int 1 if verified, 0 if incorrect, -1 on error.
 */
function payswarm_raw_verify($data, $signature, $pem) {
  if(defined('OPENSSL_ALGO_SHA256')) {
    return openssl_verify($data, $signature, $pem, OPENSSL_ALGO_SHA256);
  }

  // load public key from PEM
  $pkey = openssl_pkey_get_public($pem);
  if($pkey === false) {
    throw new Exception('PaySwarm Security Exception: ' .
      'Failed to load the public key.');
  }

  // decrypt encrypted digest info
  $rval = openssl_public_decrypt(
    $signature, $decrypted, $pkey, OPENSSL_PKCS1_PADDING);
  if(!$rval) {
    return 0;
  }

  // free public key resource
  openssl_pkey_free($pkey);

  // manually do pkcs1 v1.5 encoding (DigestInfo) for comparison
  $hash = hash('sha256', $data);
  $digestInfo = pack('H*', '3031300d060960864801650304020105000420' . $hash);

  // compare decrypted digest info to constructed one
  return ($decrypted === $digestInfo) ? 1 : 0;
}

/* end of file, omit ?> */
