// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 * @externs
 */

/**
 * @see https://drafts.fxtf.org/geometry-1/#domrectreadonly
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
class DOMRectReadOnly {
  /**
   * @param {number} x
   * @param {number} y
   * @param {number} width
   * @param {number} height
   */
  constructor(x, y, width, height) {
    /** @type {number} */
    this.x;
    /** @type {number} */
    this.y;
    /** @type {number} */
    this.width;
    /** @type {number} */
    this.height;
    /** @type {number} */
    this.top;
    /** @type {number} */
    this.right;
    /** @type {number} */
    this.bottom;
    /** @type {number} */
    this.left;
  }

  /**
   * @param {{x: number, y: number, width: number, height: number}=} rectangle
   * @return {DOMRectReadOnly}
   */
  fromRect(rectangle) {}
}

/**
 * @see https://wicg.github.io/ResizeObserver/#resizeobserverentry
 * @typedef {{contentRect: DOMRectReadOnly,
 *            target: Element}}
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
let ResizeObserverEntry;

/**
 * @see https://wicg.github.io/ResizeObserver/#api
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
class ResizeObserver {
  /**
   * @param {!function(Array<ResizeObserverEntry>, ResizeObserver)} callback
   */
  constructor(callback) {}

  disconnect() {}

  /** @param {Element} target */
  observe(target) {}

  /** @param {Element} target */
  unobserve(target) {}
}

/**
 * @see
 * https://www.polymer-project.org/2.0/docs/api/namespaces/Polymer.RenderStatus
 * Queue a function call to be run before the next render.
 * @param {!Element} element The element on which the function call is made.
 * @param {!function()} fn The function called on next render.
 * @param {...*} args The function arguments.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.RenderStatus.beforeNextRender = function(element, fn, args) {};

/**
 * @see
 * https://github.com/tc39/proposal-bigint
 * This supports wrapping and operating on arbitrarily large integers.
 *
 * @param {number} value
 */
let BigInt = function(value) {};

/**
 * TODO(manukh): Remove this once it is added to Closure Compiler itself.
 * @see https://w3c.github.io/clipboard-apis/#async-clipboard-api
 * @interface
 */
function Clipboard() {}

/**
 * @return {!Promise<string>}
 */
Clipboard.prototype.readText = function() {};

/**
 * @param {string} text
 * @return {!Promise<void>}
 */
Clipboard.prototype.writeText = function(text) {};

/** @const {!Clipboard} */
Navigator.prototype.clipboard;

/**
 * TODO(manukh): remove this once it is added to Closure Compiler itself.
 * @see https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap
 * @param {?function(this:S, T, number, !Array<T>): R} callback
 * @param {S=} opt_this
 * @return {!Array<R>}
 * @this {IArrayLike<T>|string}
 * @template T,S,R
 * @see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/flatMap
 */
Array.prototype.flatMap = function(callback, opt_this) {};

/**
 * @param {string} name
 * @param {boolean=} force
 * @return {boolean}
 * @throws {DOMException}
 * @see https://dom.spec.whatwg.org/#dom-element-toggleattribute
 */
Element.prototype.toggleAttribute = function(name, force) {};
