/*
 * PaySwarm JavaScript library.
 *
 * Copyright 2010 Digital Bazaar, Inc.
 * License: LGPLv3.
 */

/**
 * Updates the PaySwarm Authority webservice URL fields in the administrative
 * interface.
 */
function updatePaySwarmServiceUrls()
{
   var payswarmAuthority = jQuery('#payswarm_authority').val();

   // set the other fields that depend on the PaySwarm authoirity field
   jQuery('#payswarm_authorize_url').val(
      "https://" + payswarmAuthority + "/manage/authorize");
   jQuery('#payswarm_request_url').val(
      "https://" + payswarmAuthority + "/api/3.2/oauth1/tokens/request");
   jQuery('#payswarm_access_url').val(
      "https://" + payswarmAuthority + "/api/3.2/oauth1/tokens");
   jQuery('#payswarm_contracts_url').val(
      "https://" + payswarmAuthority + "/api/3.2/oauth1/contracts");
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

