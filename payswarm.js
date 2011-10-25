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
   var assetInfo = document.getElementById('payswarm-asset-info-' + postId);
   var className = assetInfo.getAttribute('"class');

   // toggle the hidden CSS state for the attribute
   if(className === 'hidden')
   {
      assetInfo.setAttribute('class', '');
   }
   else
   {
      assetInfo.setAttribute('class', 'hidden');
   }
}

/**
 * Updates the PaySwarm Authority Configuration URL.
 *
 * @param event the event that fires the authority configuration
 */
function updateAuthorityConfigUrl(event)
{
   // extract host and port information
   var authority = document.getElementById('payswarm_authority').value;
   var hostPort = authority.match(/(http:\/\/|https:\/\/)?([^/]+)\/?/i);

   // if a valid value was found, update the configuration URL
   if(hostPort != null)
   {
      // build base URL
      var baseUrl = 'https://' + hostPort[2] + '/';

      // build config URL
      var configUrl = baseUrl + 'payswarm-v1-config';

      // update display on the screen and form element
      var configDisplay = 
         document.getElementById('payswarm_config_url_display');
      var base = 
         document.getElementById('payswarm_authority_base_url');
      configDisplay.innerText = configUrl;
      base.value = baseUrl;
   }
}
