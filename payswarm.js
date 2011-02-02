/*
 * PaySwarm JavaScript library.
 *
 * Copyright 2010-2011 Digital Bazaar, Inc.
 * License: LGPLv3.
 */

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

