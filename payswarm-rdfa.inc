<?php

// FIXME: needs internationalization
// $vars is provided externally
$vars['output'] = <<<RDFA
<div
  xmlns:com="http://purl.org/commerce#"
  xmlns:dc="http://purl.org/dc/terms/"
  xmlns:foaf="http://xmlns.com/foaf/0.1/"
  xmlns:gr="http://purl.org/goodrelations/v1#"
  xmlns:ps="http://purl.org/payswarm#"
  xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#"
  xmlns:sec="http://purl.org/security#"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema#"
  about="{$vars['asset']->{'@id'}}"
  typeof="{$vars['asset_types']}"
  class="asset-info">
  <div about="{$vars['listing']->{'@id'}}" typeof="{$vars['listing_types']}">
    <form action="{$vars['access_url']}" method="post"
      onsubmit="javascript:showAuthorityPopup(this);">
      <button class="purchase-button">
        <img alt="{$vars['text_access']}" src="{$vars['pslogo_url']}" />
        {$vars['text_access']}
      </button>
      <span rel="ps:asset" resource="{$vars['asset']->{'@id'}}">
        {$vars['text_view_full_article']}
      </span>
      <span rel="com:payee" resource="{$vars['payee']->{'@id'}}">
        <span property="com:currency" content="USD">$</span>
        <abbr property="com:rate" title="{$vars['payee']->{'com:rate'}}"
          content="{$vars['payee']->{'com:rate'}}">{$vars['payee_rate']}
        </abbr>
      </span>
      <span class="view-more"
        onclick="javascript:toggleAssetInformation($post_id);">
        {$vars['text_view_offer_details']}
      </span>
    </form>
  </div>
  <div id="payswarm-asset-info-$post_id" class="hidden">
    <a rel="ps:contentUrl" href="{$vars['asset']->{'ps:contentUrl'}}"
      property="dc:title">
      {$vars['post_title']}
    </a> by
    <span rel="dc:creator">
      <span property="foaf:name">{$vars['post_author']}</span>
    </span>.
    Offered by
    <a rel="ps:assetProvider"
      href="{$vars['asset']->{'ps:assetProvider'}}">us</a>.
    <span about="{$vars['listing']->{'@id'}}">The offer to get access to this
      <span property="ps:assetHash"
        content="{$vars['listing']->{'ps:assetHash'}}">post</span>
      under <a rel="ps:license" href="{$vars['listing']->{'ps:license'}}"
        property="ps:licenseHash"
        content="{$vars['listing']->{'ps:licenseHash'}}">this license</a>
      is valid from <abbr property="ps:validFrom"
        title="{$vars['listing']->{'ps:validFrom'}}"
        content="{$vars['listing']->{'ps:validFrom'}}"
        datatype="xsd:dateTime">today</abbr>
      until <abbr property="ps:validUntil"
         title="{$vars['listing']->{'ps:validUntil'}}"
         content="{$vars['listing']->{'ps:validUntil'}}"
         datatype="xsd:dateTime">tomorrow</abbr>.
    </span>
    <span rel="sec:signature">
      <span typeof="{$vars['asset']->{'sec:signature'}->{'@type'}}">
        This <a href="{$vars['asset']->{'@id'}}">asset</a> was
        <abbr property="sec:signatureValue"
          content="{$vars['asset']->{'sec:signature'}->{'sec:signatureValue'}}"
          title="{$vars['asset']->{'sec:signature'}->{'sec:signatureValue'}}">
          digitally signed</abbr>
        using <a rel="dc:creator"
          href="{$vars['asset']->{'sec:signature'}->{'dc:creator'}}">
          this key</a>
        at <span property="dc:created" datatype="xsd:dateTime">
          {$vars['asset']->{'sec:signature'}->{'dc:created'}}</span>.
      </span>
    </span>
    <span about="{$vars['listing']->{'@id'}}">
      <span rel="com:payeeRule">
        <span typeof="com:PayeeRule">
          <span rel="com:destinationOwnerType" resource="ps:Authority"></span>
          <span rel="com:rateType" resource="com:Percentage"></span>
          <span rel="com:rateContext" resource="com:Inclusive"></span>
          <span rel="com:rateContext" resource="com:Tax"></span>
          <span rel="com:rateContext" resource="com:TaxExempt"></span>
          <span property="com:maximumRate"
            content="{$vars['payee_rule']->{'com:maximumRate'}}"></span>
        </span>
      </span>
      <span rel="sec:signature">
        <span typeof="{$vars['listing']->{'sec:signature'}->{'@type'}}">
          This <a href="{$vars['listing']->{'@id'}}">listing</a> was
          <abbr property="sec:signatureValue"
            content="{$vars['listing']->{'sec:signature'}->{'sec:signatureValue'}}"
            title="{$vars['listing']->{'sec:signature'}->{'sec:signatureValue'}}">
            digitally signed</abbr>
          using <a rel="dc:creator"
            href="{$vars['listing']->{'sec:signature'}->{'dc:creator'}}">
            this key</a>
          at <span property="dc:created" datatype="xsd:dateTime">
            {$vars['listing']->{'sec:signature'}->{'dc:created'}}</span>.
        </span>
      </span>
    </span>
    <span about="{$vars['payee']->{'@id'}}"
      typeof="{$vars['payee']->{'@type'}}">
      <span property="com:payeePosition" datatype="xsd:integer" content="0">
      </span>
      <a rel="com:rateType" href="http://purl.org/commerce#FlatAmount">
        Flat-rate payment</a> will be deposited into
      <a rel="com:destination"
        href="{$vars['payee']->{'com:destination'}}">this account</a>.
      The memo field for the payment will read:
        &quot;<span property="rdfs:comment">
          {$vars['payee']->{'rdfs:comment'}}
        </span>&quot;
    </span>
  </div>
</div>
RDFA;

/* end of file, omit ?> */
