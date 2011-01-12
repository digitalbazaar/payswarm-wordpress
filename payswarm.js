/*
 * PaySwarm JavaScript library.
 *
 * Copyright 2010 Digital Bazaar, Inc.
 * License: LGPLv3.
 */

/**
 * Converts a JSON-LD URL to a regular string for use in the interface.
 * 
 * @param url the JSON-LD formatted URL to convert to a regular string.
 *    It is also safe to pass a raw URL to this method.
 * @return The URL as a regular string value, not wrapped in '<' and '>'.
 */
function JsonldUrlToString(url)
{
   var rval = url;
   
   if(url[0] == '<' && url[url.length - 1] == '>')
   {
      rval = url.substr(1, url.length - 2);
   }
   
   return rval;
}

/**
 * Updates the given PaySwarm authority configuration fields in the
 * administration UI.
 */
function updateAuthorityConfig()
{
   // FIXME: Start loading spinner

   jQuery.post(ajaxurl, 
      {
         action: "payswarm_get_authority_config",
         config_url: jQuery("#payswarm_authority").val()
      },
      function(data, textStatus, xhr)
      {
         // convert data to JSON object
         var cfg = JSON.parse(data);
         if("<http://purl.org/payswarm/webservices#oAuthAuthorize>" in cfg &&
            "<http://purl.org/payswarm/webservices#oAuthRequest>" in cfg &&
            "<http://purl.org/payswarm/webservices#oAuthToken>" in cfg &&
            "<http://purl.org/payswarm/webservices#oAuthContract>" in cfg)
         {
            // set the other fields that depend on the PaySwarm authoirity field
            jQuery('#payswarm_authorize_url').val(
               JsonldUrlToString(cfg[
                  "<http://purl.org/payswarm/webservices#oAuthAuthorize>"]));
            jQuery('#payswarm_request_url').val(
               JsonldUrlToString(
                  cfg["<http://purl.org/payswarm/webservices#oAuthRequest>"]));
            jQuery('#payswarm_access_url').val(
               JsonldUrlToString(
                  cfg["<http://purl.org/payswarm/webservices#oAuthToken>"]));
            jQuery('#payswarm_contracts_url').val(
               JsonldUrlToString(
                  cfg["<http://purl.org/payswarm/webservices#oAuthContract>"]));
            
            // FIXME: Stop loading spinner, show VALID message
         }
         else
         {
            // FIXME: Stop loading spinner, show INVALID message
         }
      }
   );
}

/**
 * Expands the asset information when the title is clicked.
 *
 * @param postId the identifier for the post
 */
function toggleAssetInformation(postId)
{
   var assetInfo = document.getElementById("payswarm-asset-info-" + postId);
   var className = assetInfo.getAttribute("class");

   // toggle the hidden CSS state for the attribute
   if(className == "hidden")
   {
      assetInfo.setAttribute("class", "");
   }
   else
   {
      assetInfo.setAttribute("class", "hidden");
   }
}

