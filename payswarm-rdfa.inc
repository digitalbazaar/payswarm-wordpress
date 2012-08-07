<?php

// FIXME: needs internationalization
// $vars is provided externally
$vars['output'] = <<<RDFA
<div
  xmlns:com="http://purl.org/commerce#"
  xmlns:dc="http://purl.org/dc/terms/"
  xmlns:vcard="http://www.w3.org/2006/vcard/ns#"
  xmlns:gr="http://purl.org/goodrelations/v1#"
  xmlns:pto="http://www.productontology.org/id/"
  xmlns:ps="http://purl.org/payswarm#"
  xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#"
  xmlns:sec="http://purl.org/security#"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema#"
  about="{$vars['asset']->id}"
  typeof="{$vars['asset_types']}"
  class="asset-info">
  <div about="{$vars['listing']->id}" typeof="{$vars['listing_types']}">
    <form action="{$vars['access_url']}" method="post"
      onsubmit="javascript:showAuthorityPopup(this);">
      <button class="purchase-button">
        <img alt="{$vars['text_access']}" src="{$vars['pslogo_url']}" />
        {$vars['text_access']}
      </button>
      <span rel="ps:asset" resource="{$vars['asset']->id}">
        {$vars['text_view_full_article']}
      </span>
      <span rel="com:payee" resource="{$vars['payee']->id}">
        <span>$</span>
        <abbr property="com:payeeRate" title="{$vars['payee']->payeeRate}"
          content="{$vars['payee']->payeeRate}">{$vars['payee_rate']}
        </abbr>
      </span>
      <span class="view-more"
        onclick="javascript:toggleAssetInformation($post_id);">
        {$vars['text_view_offer_details']}
      </span>
    </form>
  </div>
  <div id="payswarm-asset-info-$post_id" class="hidden">
    <a rel="ps:assetContent" href="{$vars['asset']->assetContent}"
      property="dc:title">{$vars['post_title']}</a> by
    <span rel="dc:creator">
      <span property="vcard:fn">{$vars['post_author']}</span>
    </span>.
    Offered by
    <a rel="ps:assetProvider"
      href="{$vars['asset']->assetProvider}">us</a>.
    <span about="{$vars['listing']->id}">The offer to get access to this
      <span property="ps:assetHash"
        content="{$vars['listing']->assetHash}">post</span>
      under <a rel="ps:license" href="{$vars['listing']->license}"
        property="ps:licenseHash"
        content="{$vars['listing']->licenseHash}">this license</a>
      is valid from <abbr property="ps:validFrom"
        title="{$vars['listing']->validFrom}"
        content="{$vars['listing']->validFrom}"
        datatype="xsd:dateTime">today</abbr>
      until <abbr property="ps:validUntil"
         title="{$vars['listing']->validUntil}"
         content="{$vars['listing']->validUntil}"
         datatype="xsd:dateTime">tomorrow</abbr>.
    </span>
    <span rel="sec:signature">
      <span typeof="{$vars['asset']->signature->type}">
        This <a href="{$vars['asset']->id}">asset</a> was
        <abbr property="sec:signatureValue"
          content="{$vars['asset']->signature->signatureValue}"
          title="{$vars['asset']->signature->signatureValue}">
          digitally signed</abbr>
        using <a rel="dc:creator"
          href="{$vars['asset']->signature->creator}">
          this key</a>
        at <span property="dc:created" datatype="xsd:dateTime">{$vars['asset']->signature->created}</span>.
      </span>
    </span>
    <span about="{$vars['listing']->id}">
      <span rel="com:payeeRule">
        <span typeof="com:PayeeRule">
          <span rel="com:accountOwnerType" resource="ps:Authority"></span>
          <span rel="com:payeeRateType" resource="com:Percentage"></span>
          <span rel="com:payeeRateContext" resource="com:Inclusive"></span>
          <span rel="com:payeeRateContext" resource="com:Tax"></span>
          <span rel="com:payeeRateContext" resource="com:TaxExempt"></span>
          <span property="com:maximumPayeeRate"
            content="{$vars['payee_rule']->maximumPayeeRate}"></span>
        </span>
      </span>
      <span rel="sec:signature">
        <span typeof="{$vars['listing']->signature->type}">
          This <a href="{$vars['listing']->id}">listing</a> was
          <abbr property="sec:signatureValue"
            content="{$vars['listing']->signature->signatureValue}"
            title="{$vars['listing']->signature->signatureValue}">
            digitally signed</abbr>
          using <a rel="dc:creator"
            href="{$vars['listing']->signature->creator}">
            this key</a>
          at <span property="dc:created" datatype="xsd:dateTime">{$vars['listing']->signature->created}</span>.
        </span>
      </span>
    </span>
    <span about="{$vars['payee']->id}"
      typeof="{$vars['payee']->type}">
      <span property="com:payeePosition" datatype="xsd:integer" content="0">
      </span>
      <a rel="com:payeeRateType" href="http://purl.org/commerce#FlatAmount">
        Flat-rate payment</a> will be deposited into
      <a rel="com:destination"
        href="{$vars['payee']->destination}">this account</a>.
      The memo field for the payment will read:
        &quot;
        <span property="rdfs:comment">{$vars['payee']->comment}</span>
        &quot;
    </span>
  </div>
</div>
RDFA;

/* end of file, omit ?> */
