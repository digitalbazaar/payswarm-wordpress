/*
 * PaySwarm JavaScript library.
 *
 * Copyright 2010-2012 Digital Bazaar, Inc.
 * License: LGPLv3.
 */

/**
 * Expands the asset information when the title is clicked.
 *
 * @param postId the identifier for the post
 */
function toggleAssetInformation(postId) {
  var assetInfo = document.getElementById('payswarm-asset-info-' + postId);
  var className = assetInfo.getAttribute('class');

  // toggle the hidden CSS state for the attribute
  if(className === 'hidden') {
    assetInfo.setAttribute('class', '');
  }
  else {
    assetInfo.setAttribute('class', 'hidden');
  }
}

/**
 * Shows the PaySwarm Authority registration pop up.
 */
function showRegisterPopup() {
  var width = 700;
  var height = 600;
  var popup = window.open('', 'registerpopup',
    'left=' + ((screen.width-width)/2) +
    ',top=' + ((screen.height-height)/2) +
    ',width=' + width +
    ',height=' + height +
    ',resizeable,scrollbars');
  document.getElementById('register').target = 'registerpopup';
}

/**
 * Closes any pop up window and loads the given url.
 *
 * @param url the URL to load in the parent.
 */
function closePopup(url) {
  if(window.opener === null) {
    window.location = url;
  }
  else {
    window.close();
    window.opener.location = url;
  }
}
