/*********************************************************************
This file contains support javascript functions used by a browser.
They are much easier to write here, in javascript,
then in C using the js api.
And it is portable amongst all js engines.
This file is converted into a C string and compiled and run
at the start of each javascript window.
Please take advantage of this machinery and put functions here,
even prototypes and getter / setter support functions,
whenever it makes sense to do so.
The classes are created first, so that you can write meaningful prototypes here.
*********************************************************************/

/* Some visual attributes of the window.
 * These are just guesses.
 * Better to have something than nothing at all. */
height = 768;
width = 1024;
// document.status is removed because it creates a conflict with
// the status property of the XMLHttpRequest implementation
defaultStatus = 0;
returnValue = true;
menubar = true;
scrollbars = true;
toolbar = true;
resizable = true;
directories = false;
name = "unspecifiedFrame";

document.bgcolor = "white";
document.readyState = "loading";
document.nodeType = 9;
document.implementation = {};
// pages seem to want document.style to exist
document.style = {"bgcolor":"white"};

screen = new Object;
screen.height = 768;
screen.width = 1024;
screen.availHeight = 768;
screen.availWidth = 1024;
screen.availTop = 0;
screen.availLeft = 0;

/* some base arrays - lists of things we'll probably need */
document.heads = new Array;
document.bases = new Array;
document.links = new Array;
document.metas = new Array;
document.bodies = new Array;
document.forms = new Array;
document.elements = new Array;
document.anchors = new Array;
document.divs = new Array;
document.htmlobjs = new Array;
document.scripts = new Array;
document.paragraphs = new Array;
document.tables = new Array;
document.spans = new Array;
document.images = new Array;
document.areas = new Array;
frames = new Array;

document.getElementsByTagName = function(s) { 
s = s.toLowerCase();
return document.gebtn$(this, s);
}
document.gebtn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebtn$(c, s));
}
}
return a;
}

document.getElementsByName = function(s) { 
s = s.toLowerCase();
return document.gebn$(this, s);
}
document.gebn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.name && top.name.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebn$(c, s));
}
}
return a;
}

document.getElementsByClassName = function(s) { 
s = s.toLowerCase();
return document.gebcn$(this, s);
}
document.gebcn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.className && top.className.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebcn$(c, s));
}
}
return a;
}

Head.prototype.getElementsByTagName = document.getElementsByTagName;
Head.prototype.getElementsByName = document.getElementsByName;
Head.prototype.getElementsByClassName = document.getElementsByClassName;
Body.prototype.getElementsByTagName = document.getElementsByTagName;
Body.prototype.getElementsByName = document.getElementsByName;
Body.prototype.getElementsByClassName = document.getElementsByClassName;
Form.prototype.getElementsByTagName = document.getElementsByTagName;
Form.prototype.getElementsByName = document.getElementsByName;
Form.prototype.getElementsByClassName = document.getElementsByClassName;
Element.prototype.getElementsByTagName = document.getElementsByTagName;
Element.prototype.getElementsByName = document.getElementsByName;
Element.prototype.getElementsByClassName = document.getElementsByClassName;
Anchor.prototype.getElementsByTagName = document.getElementsByTagName;
Anchor.prototype.getElementsByName = document.getElementsByName;
Anchor.prototype.getElementsByClassName = document.getElementsByClassName;
Div.prototype.getElementsByTagName = document.getElementsByTagName;
Div.prototype.getElementsByName = document.getElementsByName;
Div.prototype.getElementsByClassName = document.getElementsByClassName;
Script.prototype.getElementsByTagName = document.getElementsByTagName;
Script.prototype.getElementsByName = document.getElementsByName;
Script.prototype.getElementsByClassName = document.getElementsByClassName;
P.prototype.getElementsByTagName = document.getElementsByTagName;
P.prototype.getElementsByName = document.getElementsByName;
P.prototype.getElementsByClassName = document.getElementsByClassName;
Lister.prototype.getElementsByTagName = document.getElementsByTagName;
Lister.prototype.getElementsByName = document.getElementsByName;
Lister.prototype.getElementsByClassName = document.getElementsByClassName;
Listitem.prototype.getElementsByTagName = document.getElementsByTagName;
Listitem.prototype.getElementsByName = document.getElementsByName;
Listitem.prototype.getElementsByClassName = document.getElementsByClassName;
Table.prototype.getElementsByTagName = document.getElementsByTagName;
Table.prototype.getElementsByName = document.getElementsByName;
Table.prototype.getElementsByClassName = document.getElementsByClassName;
Tbody.prototype.getElementsByTagName = document.getElementsByTagName;
Tbody.prototype.getElementsByName = document.getElementsByName;
Tbody.prototype.getElementsByClassName = document.getElementsByClassName;
Trow.prototype.getElementsByTagName = document.getElementsByTagName;
Trow.prototype.getElementsByName = document.getElementsByName;
Trow.prototype.getElementsByClassName = document.getElementsByClassName;
Cell.prototype.getElementsByTagName = document.getElementsByTagName;
Cell.prototype.getElementsByName = document.getElementsByName;
Cell.prototype.getElementsByClassName = document.getElementsByClassName;
Span.prototype.getElementsByTagName = document.getElementsByTagName;
Span.prototype.getElementsByName = document.getElementsByName;
Span.prototype.getElementsByClassName = document.getElementsByClassName;

document.idMaster = new Object;
document.getElementById = function(s) { 
/* take advantage of the js hash lookup */
return document.idMaster[s]; 
}

/* originally ms extension pre-DOM, we don't fully support it
* but offer the document.all.tags method because that was here already */
document.all = new Object;
document.all.tags = function(s) { 
return document.gebtn$(document.body, s.toLowerCase());
}

/* document.createElement is a native wrapper around this function */
document.crel$$ = function(s) { 
var c;
var t = s.toLowerCase();
switch(t) { 
case "body":
c = new Body();
break;
case "a":
c = new Anchor();
break;
case "image":
t = "img";
case "img":
c = new Image();
break;
case "script":
c = new Script();
break;
case "div":
c = new Div();
break;
case "p":
c = new P();
break;
case "table":
c = new Table();
break;
case "tbody":
c = new Tbody();
break;
case "tr":
c = new Trow();
break;
case "td":
c = new Cell();
break;
case "select":
/* select and radio are special form elements in that they are intrinsically
 * arrays, with all the options as array elements,
 * and also "options" or "childNodes" linked to itself
 * so it looks like it has children in the usual way. */
c = new Array;
c.nodeName = t;
c.options = c;
c.childNodes = c;
c.selectedIndex = -1;
c.value = "";
// no style, and childNodes already self-linked, so just return.
return c;
case "option":
c = new Option();
break;
default:
/* $puts$("createElement default " + s); */
c = new Element();
}
/* ok, for some element types this perhaps doesn't make sense,
* but for most visible ones it does and I doubt it matters much */
c.style = new Object;
c.childNodes = new Array;
c.attributes = new Array;
c.nodeName = t;
c.nodeType = 1;
c.nodeValue = undefined;
c.class = new String;
c.className = new String;
c.ownerDocument = document;
c.tagName = t;

return c;
} 

document.createTextNode = function(t) {
var c = new TextNode(t);
c.nodeName = "text";
c.nodeValue = t;
c.nodeType=3;
c.ownerDocument = document;
c.style = new Object;
c.tagName = "text";
c.className = new String;
return c;
}

document.createDocumentFragment = function() {
var c = document.createElement("fragment");
c.nodeType = 11;
return c;
}

document.createComment = function() {
var c = document.createElement("comment");
c.nodeType = 8;
return c;
}

// window.alert is a simple wrapper around native puts.
function alert(s) { $puts$(s); }

/* window.open is the same as new window, just pass the args through */
function open() {
return Window.apply(this, arguments);
}

var $urlpro = URL.prototype;

/* rebuild the href string from its components.
 * Call this when a component changes.
 * All components are strings, except for port,
 * and all should be defined, even if they are empty. */
$urlpro.rebuild = function() {
var h = "";
if(this.protocol$val.length) {
// protocol includes the colon
h = this.protocol$val;
var plc = h.toLowerCase();
if(plc != "mailto:" && plc != "telnet:" && plc != "javascript:")
h += "//";
}
if(this.host$val.length) {
h += this.host$val;
} else if(this.hostname$val.length) {
h += this.hostname$val;
if(this.port$val != 0)
h += ":" + this.port$val;
}
if(this.pathname$val.length) {
// pathname should always begin with /, should we check for that?
if(!this.pathname$val.match(/^\//))
h += "/";
h += this.pathname$val;
}
if(this.search$val.length) {
// search should always begin with ?, should we check for that?
h += this.search$val;
}
if(this.hash$val.length) {
// hash should always begin with #, should we check for that?
h += this.hash$val;
}
this.href$val = h;
};

// No idea why we can't just assign the property directly.
// $urlpro.protocol = { ... };
Object.defineProperty($urlpro, "protocol", {
  get: function() {return this.protocol$val; },
  set: function(v) { this.protocol$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "pathname", {
  get: function() {return this.pathname$val; },
  set: function(v) { this.pathname$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "search", {
  get: function() {return this.search$val; },
  set: function(v) { this.search$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "hash", {
  get: function() {return this.hash$val; },
  set: function(v) { this.hash$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "port", {
  get: function() {return this.port$val; },
  set: function(v) { this.port$val = v;
if(this.hostname$val.length)
this.host$val = this.hostname$val + ":" + v;
this.rebuild(); }
});

Object.defineProperty($urlpro, "hostname", {
  get: function() {return this.hostname$val; },
  set: function(v) { this.hostname$val = v;
if(this.port$val)
this.host$val = v + ":" +  this.port$val;
this.rebuild(); }
});

Object.defineProperty($urlpro, "host", {
  get: function() {return this.host$val; },
  set: function(v) { this.host$val = v;
if(v.match(/:/)) {
this.hostname$val = v.replace(/:.*/, "");
this.port$val = v.replace(/^.*:/, "");
/* port has to be an integer */
this.port$val = parseInt(this.port$val);
} else {
this.hostname$val = v;
this.port$val = 0;
}
this.rebuild(); }
});

var prot$port = {
http: 80,
https: 443,
pop3: 110,
pop3s: 995,
imap: 220,
imaps: 993,
smtp: 25,
submission: 587,
smtps: 465,
proxy: 3128,
ftp: 21,
sftp: 22,
scp: 22,
ftps: 990,
tftp: 69,
gopher: 70,
finger: 79,
telnet: 23,
smb: 139
};

/* returns default port as an integer, based on protocol */
function default$port(p) {
var port = 0;
p = p.toLowerCase().replace(/:/, "");
if(prot$port.hasOwnProperty(p))
port = parseInt(prot$port[p]);
return port;
}

Object.defineProperty($urlpro, "href", {
  get: function() {return this.href$val; },
  set: function(v) { this.href$val = v;
// initialize components to empty,
// then fill them in from href if they are present */
this.protocol$val = "";
this.hostname$val = "";
this.port$val = 0;
this.host$val = "";
this.pathname$val = "";
this.search$val = "";
this.hash$val = "";
if(v.match(/^[a-zA-Z]*:/)) {
this.protocol$val = v.replace(/:.*/, "");
this.protocol$val += ":";
v = v.replace(/^[a-zA-z]*:\/*/, "");
}
if(v.match(/[/#?]/)) {
/* contains / ? or # */
this.host$val = v.replace(/[/#?].*/, "");
v = v.replace(/^[^/#?]*/, "");
} else {
/* no / ? or #, the whole thing is the host, www.foo.bar */
this.host$val = v;
v = "";
}
if(this.host$val.match(/:/)) {
this.hostname$val = this.host$val.replace(/:.*/, "");
this.port$val = this.host$val.replace(/^.*:/, "");
/* port has to be an integer */
this.port$val = parseInt(this.port$val);
} else {
this.hostname$val = this.host$val;
// should we be filling in a default port here?
this.port$val = default$port(this.protocol$val);
}
// perhaps set protocol to http if it looks like a url?
// as in edbrowse foo.bar.com
// Ends in standard tld, or looks like an ip4 address, or starts with www.
if(this.protocol$val == "" &&
(this.hostname$val.match(/\.(com|org|net|info|biz|gov|edu|us|uk|ca|au)$/) ||
this.hostname$val.match(/^\d+\.\d+\.\d+\.\d+$/) ||
this.hostname$val.match(/^www\..*\.[a-zA-Z]{2,}$/))) {
this.protocol$val = "http:";
if(this.port$val == 0)
this.port$val = 80;
}
if(v.match(/[#?]/)) {
this.pathname$val = v.replace(/[#?].*/, "");
v = v.replace(/^[^#?]*/, "");
} else {
this.pathname$val = v;
v = "";
}
if(this.pathname$val == "")
this.pathname$val = "/";
if(v.match(/#/)) {
this.search$val = v.replace(/#.*/, "");
this.hash$val = v.replace(/^[^#]*/, "");
} else {
this.search$val = v;
}
}
});

URL.prototype.toString = function() { 
return this.href$val;
}
URL.prototype.indexOf = function(s) { 
return this.toString().indexOf(s);
}
URL.prototype.lastIndexOf = function(s) { 
return this.toString().lastIndexOf(s);
}
URL.prototype.substring = function(from, to) { 
return this.toString().substring(from, to);
}
// pages expect both substring and substr 
URL.prototype.substr = function(from, to) {
return this.toString().substr(from, to);
}
URL.prototype.toLowerCase = function() { 
return this.toString().toLowerCase();
}
URL.prototype.toUpperCase = function() { 
return this.toString().toUpperCase();
}
URL.prototype.match = function(s) { 
return this.toString().match(s);
}
URL.prototype.replace = function(s, t) { 
return this.toString().replace(s, t);
}
URL.prototype.split = function(s) {
return this.toString().split(s);
}

/*********************************************************************
This is our addEventListener function.
It is bound to window, which is ok because window has such a function
to listen to load and unload.
Later on we will bind it to document and to other elements via
element.addEventListener = addEventListener
Or maybe URL.prototype.addEventListener = addEventListener
to cover all the hyperlinks in one go.
first arg is a string like click, second arg is a js handler,
Third arg is not used cause I don't understand it.
*********************************************************************/

function addEventListener(ev, handler, notused)
{
ev = "on" + ev;
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
if(!this[evarray]) {
/* attaching the first handler */
var a = new Array;
/* was there already a function from before? */
if(this[ev]) {
this[evorig] = this[ev];
this[ev] = undefined;
}
this[evarray] = a;
eval(
'this.' + ev + ' = function(){ var a = this.' + evarray + '; if(this.' + evorig + ') this.' + evorig + '(); for(var i = 0; i<a.length; ++i) {a[i]();} };');
}
this[evarray].push(handler);
}

// here is the counterpart to add
// what if every handler is removed and there is an empty array?
// the assumption is that this is not a problem
function removeEventListener(ev, handler, notused)
{
ev = "on" + ev;
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
// remove original html handler after other events have been added.
if(this[evorig] == handler) {
delete this[evorig];
return;
}
// remove original html handler when no other events have been added.
if(this[ev] == handler) {
delete this[ev];
return;
}
// If other events have been added, check through the array.
if(this[evarray]) {
var a = this[evarray]; // shorthand
for(var i = 0; i<a.length; ++i)
if(a[i] == handler) {
a.splice(i, 1);
return;
}
}
}

/* For grins let's put in the other standard. */

function attachEvent(ev, handler)
{
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
if(!this[evarray]) {
/* attaching the first handler */
var a = new Array;
/* was there already a function from before? */
if(this[ev]) {
this[evorig] = this[ev];
this[ev] = undefined;
}
this[evarray] = a;
eval(
'this.' + ev + ' = function(){ var a = this.' + evarray + '; if(this.' + evorig + ') this.' + evorig + '(); for(var i = 0; i<a.length; ++i) a[i](); };');
}
this[evarray].push(handler);
}

document.addEventListener = window.addEventListener;
document.attachEvent = window.attachEvent;
document.removeEventListener = window.removeEventListener;
Body.prototype.addEventListener = window.addEventListener;
Body.prototype.attachEvent = window.attachEvent;
Body.prototype.removeEventListener = window.removeEventListener;
Form.prototype.addEventListener = window.addEventListener;
Form.prototype.attachEvent = window.attachEvent;
Form.prototype.removeEventListener = window.removeEventListener;
Element.prototype.addEventListener = window.addEventListener;
Element.prototype.attachEvent = window.attachEvent;
Element.prototype.removeEventListener = window.removeEventListener;
Anchor.prototype.addEventListener = window.addEventListener;
Anchor.prototype.attachEvent = window.attachEvent;
Anchor.prototype.removeEventListener = window.removeEventListener;

/*********************************************************************
The functions apch1$ and apch2$ are native. They perform appendChild in js.
The first has no side effects, because the linkage was already performed
within edbrowse via html, and a linkage side effect would only confuse things.
The second, apch2$, has side effects, as js code calls appendChild
and those links have to pass back to edbrowse.
But, the actual function appendChild makes another check;
if the child is already linked into the tree, then we have to unlink it first,
before we put it somewhere else.
This is a call to removeChild, which unlinks in js,
and passses the remove side effect back to edbrowse.
The same reasoning holds for insertBefore.
*********************************************************************/

document.appendChild = function(c) {
if(c.parentNode) c.parentNode.removeChild(c);
return this.apch2$(c);
}

document.insertBefore = function(c, t) {
if(c.parentNode) c.parentNode.removeChild(c);
return this.insbf$(c, t);
}

document.childNodes = new Array;
Object.defineProperty(document, "firstChild", {
get: function() { return document.childNodes[0]; }
});
Object.defineProperty(document, "lastChild", {
get: function() { return document.childNodes[document.childNodes.length-1]; }
});
Object.defineProperty(document, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(document, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

document.hasChildNodes = function() { return (this.childNodes.length > 0); }
document.replaceChild = function(newc, oldc) {
var lastentry;
var l = this.childNodes.length;
var nextinline;
for(var i=0; i<l; ++i) {
if(this.childNodes[i] != oldc)
continue;
if(i == l-1)
lastentry = true;
else {
lastentry = false;
nextinline = this.childNodes[i+1];
}
this.removeChild(oldc);
if(lastentry)
this.appendChild(newc);
else
this.insertBefore(newc, nextinline);
break;
}
}
document.getAttribute = function(name) { return this[name.toLowerCase()]; }
document.hasAttribute = function(name) { if (this[name.toLowerCase()]) return true; else return false; }
// Sets the attribute in 3 different ways, the third using attributes as an array.
// This may be overkill. I don't know.
document.setAttribute = function(name, v) { 
var n = name.toLowerCase();
this[n] = v; 
this.attributes[n] = v;
this.attributes.push(n);
}
// Removes the attribute in 3 different ways, the third using attributes as an array.
// This may be overkill. I don't know.
document.removeAttribute = function(name) {
    var n = name.toLowerCase();
    if (this[n]) delete this[n];
if(this.attributes[n]) delete this.attributes[n];
    for (var i=this.attributes.length - 1; i >= 0; --i) {
        if (this.attributes[i] == n) {
this.attributes.splice(i, 1);
break;
}
}
}

/*********************************************************************
Notes on cloneNode:

- cloneNode creates a copy of what it is called on, yet
we are not allowed to have multiple document objects. So
is it a problem to define this on document and other kinds
of objects get that implementation in the usual way?
KC: I don't think it's a problem, because we aren't
trying to prohibit every weird or nonsensical case.  It will
tend not to happen in the wild most of the time.
KD: Well the first step of cloneNode, createNode("document"),
won't work anyways.

- Does cloneNode need to be native?  Does it need a side effect?
KC: I don't think it does, because it appears from MDC that it
is intended to be used as the first hop of a multi-step usage.
cloneNode returns a JS variable.  It's up to the developer to
use appendChild or similar, to add it to the tree.  So
cloneNode is a 'second order' routine which itself calls the
more integral work that we already did, and the first-order
routines will deal with side effects.
KD: Agree, and somewhat surprised, happily surprised.

- There is recursion involved here.  The argument, 'deep'
refers to whether or not the clone will have a literal
copy of additional depths on the tree of the original.
KC: This is the reason for having cloneNode call importNode,
and importNode calling importNode.  Also, importNode
is not tethered to any particular prototype.
*********************************************************************/

document.cloneNode = function(deep) {
return import$node (this,deep);
}

function import$node(nodeToCopy,deep)
{
var nodeToReturn;
var i;

// special case for array, which is the select node.
if(nodeToCopy instanceof Array) {
nodeToReturn = new Array;
for(i = 0; i < nodeToCopy.length; ++i)
nodeToReturn.push(import$node(nodeToCopy[i]));
} else {

nodeToReturn = document.createElement(nodeToCopy.nodeName);
if (deep && nodeToCopy.childNodes) {
for(i = 0; i < nodeToCopy.childNodes.length; ++i) {
var current_item = nodeToCopy.childNodes[i];
nodeToReturn.appendChild(import$node(current_item,true));
}
}
}

// now for the strings.
for (var item in nodeToCopy) {
if (typeof nodeToCopy[item] == 'string' && nodeToCopy[item] !== '')
nodeToReturn[item] = nodeToCopy[item];
}

// copy style object if present and its subordinate strings.
if (typeof nodeToCopy.style == "object") {
nodeToReturn.style = {};
for (var item in nodeToCopy.style){
if (typeof nodeToCopy.style[item] == 'string')
nodeToReturn.style[item] = nodeToCopy.style[item];
}
}

// copy any objects of class URL.
for (var url in nodeToCopy) {
var u = nodeToCopy[url];
if(typeof u == "object" && u instanceof URL)
nodeToReturn[url] = new URL(u.href);
}

return nodeToReturn;
}

function get_sibling(obj,direction)
{
if (typeof obj.parentNode == 'undefined') {
// need calling node to have parent and it doesn't, error
return null;
}
var pn = obj.parentNode;
var j, l;
l = pn.childNodes.length;
for (j=0; j<l; ++j)
if (pn.childNodes[j] == obj) break;
if (j == l) {
// child not found under parent, error
return null;
}
switch(direction)
{
case "previous":
return (j > 0 ? pn.childNodes[j-1] : null);
break;
case "next":
return (j < l-1 ? pn.childNodes[j+1] : null);
break;
default:
// the function should always have been called with either 'previous' or 'next' specified
return null;
}
}

/* The select element in a form is itself an array, so the above functions have
 * to be on array prototype, except appendchild is to have no side effects,
 * because select options are maintained by rebuildSelectors(), so appendChild
 * is just array.push(). */
Array.prototype.appendChild = function(child) {
// check to see if it's already there
for(var i=0; i<this.length; ++i)
if(this[i] == child)
return child;
this.push(child);return child; }
/* insertBefore maps to splice, but we have to find the element. */
/* This prototype assumes all elements are objects. */
Array.prototype.insertBefore = function(newobj, item) {
// check to see if it's already there
for(var i=0; i<this.length; ++i)
if(this[i] == newobj)
return newobj;
for(var i=0; i<this.length; ++i)
if(this[i] == item) {
this.splice(i, 0, newobj);
return newobj;
}
}
Array.prototype.removeChild = function(item) {
for(var i=0; i<this.length; ++i)
if(this[i] == item) {
this.splice(i, 1);
return;
}
}
Object.defineProperty(Array.prototype, "firstChild", {
get: function() { return this[0]; }
});
Object.defineProperty(Array.prototype, "lastChild", {
get: function() { return this[this.length-1]; }
});
Object.defineProperty(Array.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Array.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Array.prototype.hasChildNodes = document.hasChildNodes;
Array.prototype.replaceChild = document.replaceChild;
Array.prototype.getAttribute = document.getAttribute;
Array.prototype.setAttribute = document.setAttribute;
Array.prototype.hasAttribute = document.hasAttribute;
Array.prototype.removeAttribute = document.removeAttribute;

Head.prototype.appendChild = document.appendChild;
Head.prototype.apch1$ = document.apch1$;
Head.prototype.apch2$ = document.apch2$;
Head.prototype.insertBefore = document.insertBefore;
Head.prototype.insbf$ = document.insbf$;
Object.defineProperty(Head.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Head.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Head.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Head.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Head.prototype.hasChildNodes = document.hasChildNodes;
Head.prototype.removeChild = document.removeChild;
Head.prototype.replaceChild = document.replaceChild;
Head.prototype.getAttribute = document.getAttribute;
Head.prototype.setAttribute = document.setAttribute;
Head.prototype.cloneNode = document.cloneNode;
Head.prototype.hasAttribute = document.hasAttribute;
Head.prototype.removeAttribute = document.removeAttribute;

Body.prototype.appendChild = document.appendChild;
Body.prototype.apch1$ = document.apch1$;
Body.prototype.apch2$ = document.apch2$;
Body.prototype.insertBefore = document.insertBefore;
Body.prototype.insbf$ = document.insbf$;
Object.defineProperty(Body.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Body.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Body.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Body.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Body.prototype.hasChildNodes = document.hasChildNodes;
Body.prototype.removeChild = document.removeChild;
Body.prototype.replaceChild = document.replaceChild;
Body.prototype.getAttribute = document.getAttribute;
Body.prototype.setAttribute = document.setAttribute;
Body.prototype.cloneNode = document.cloneNode;
Body.prototype.hasAttribute = document.hasAttribute;
Body.prototype.removeAttribute = document.removeAttribute;

/*********************************************************************
Special functions for form and input.
If you add an input to a form, it adds under childNodes in the usual way,
but also must add in the elements[] array.
Same for insertBefore and removeChild.
Same for insertBefore and removeChild.
When adding an input element to a form,
linnk form[element.name] to that element.
*********************************************************************/

function form$name(parent, child)
{
var s;
if(typeof child.name == "string")
s = child.name;
else if(typeof child.id == "string")
s = child.id;
else return;
// Is it ok if name is "action"? I'll assume it is,
// but then there is no way to submit the form. Oh well.
parent[s] = child;
}

Form.prototype.appendChildNative = document.appendChild;
Form.prototype.appendChild = function(newobj) {
this.appendChildNative(newobj);
if(newobj.nodeName === "input" || newobj.nodeName === "select") {
this.elements.appendChild(newobj);
form$name(this, newobj);
}
}
Form.prototype.apch1$ = document.apch1$;
Form.prototype.apch2$ = document.apch2$;
Form.prototype.insbf$ = document.insbf$;
Form.prototype.insertBeforeNative = document.insertBefore;
Form.prototype.insertBefore = function(newobj, item) {
this.insertBeforeNative(newobj, item);
if(newobj.nodeName === "input" || newobj.nodeName === "select") {
// the following won't work unless item is also type input.
this.elements.insertBefore(newobj, item);
form$name(this, newobj);
}
}
Object.defineProperty(Form.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Form.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Form.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Form.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Form.prototype.hasChildNodes = document.hasChildNodes;
Form.prototype.removeChildNative = document.removeChild;
Form.prototype.removeChild = function(item) {
this.removeChildNative(item);
if(item.nodeName === "input" || item.nodeName === "select")
this.elements.removeChild(item);
}
Form.prototype.replaceChild = document.replaceChild;
Form.prototype.getAttribute = document.getAttribute;
Form.prototype.setAttribute = document.setAttribute;
Form.prototype.cloneNode = document.cloneNode;
Form.prototype.hasAttribute = document.hasAttribute;
Form.prototype.removeAttribute = document.removeAttribute;

Element.prototype.appendChild = document.appendChild;
Element.prototype.apch1$ = document.apch1$;
Element.prototype.apch2$ = document.apch2$;
Element.prototype.insertBefore = document.insertBefore;
Element.prototype.insbf$ = document.insbf$;
Object.defineProperty(Element.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Element.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Element.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Element.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Element.prototype.hasChildNodes = document.hasChildNodes;
Element.prototype.removeChild = document.removeChild;
Element.prototype.replaceChild = document.replaceChild;
Element.prototype.getAttribute = document.getAttribute;
Element.prototype.setAttribute = document.setAttribute;
Element.prototype.focus = document.focus;
Element.prototype.blur = document.blur;
Element.prototype.cloneNode = document.cloneNode;
Element.prototype.hasAttribute = document.hasAttribute;
Element.prototype.removeAttribute = document.removeAttribute;

Anchor.prototype.appendChild = document.appendChild;
Anchor.prototype.apch1$ = document.apch1$;
Anchor.prototype.apch2$ = document.apch2$;
Anchor.prototype.focus = document.focus;
Anchor.prototype.blur = document.blur;
Anchor.prototype.getAttribute = document.getAttribute;
Anchor.prototype.setAttribute = document.setAttribute;
Anchor.prototype.cloneNode = document.cloneNode;
Anchor.prototype.hasAttribute = document.hasAttribute;
Anchor.prototype.removeAttribute = document.removeAttribute;

Div.prototype.appendChild = document.appendChild;
Div.prototype.apch1$ = document.apch1$;
Div.prototype.apch2$ = document.apch2$;
Div.prototype.insertBefore = document.insertBefore;
Div.prototype.insbf$ = document.insbf$;
Object.defineProperty(Div.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Div.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Div.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Div.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Div.prototype.hasChildNodes = document.hasChildNodes;
Div.prototype.removeChild = document.removeChild;
Div.prototype.replaceChild = document.replaceChild;
Div.prototype.getAttribute = document.getAttribute;
Div.prototype.setAttribute = document.setAttribute;
Div.prototype.cloneNode = document.cloneNode;
Div.prototype.hasAttribute = document.hasAttribute;
Div.prototype.removeAttribute = document.removeAttribute;

HtmlObj.prototype.appendChild = document.appendChild;
HtmlObj.prototype.apch1$ = document.apch1$;
HtmlObj.prototype.apch2$ = document.apch2$;
HtmlObj.prototype.insertBefore = document.insertBefore;
HtmlObj.prototype.insbf$ = document.insbf$;
Object.defineProperty(HtmlObj.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(HtmlObj.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(HtmlObj.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(HtmlObj.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});


HtmlObj.prototype.hasChildNodes = document.hasChildNodes;
HtmlObj.prototype.removeChild = document.removeChild;
HtmlObj.prototype.replaceChild = document.replaceChild;
HtmlObj.prototype.getAttribute = document.getAttribute;
HtmlObj.prototype.setAttribute = document.setAttribute;
HtmlObj.prototype.cloneNode = document.cloneNode;
HtmlObj.prototype.hasAttribute = document.hasAttribute;
HtmlObj.prototype.removeAttribute = document.removeAttribute;

Script.prototype.getAttribute = document.getAttribute;
Script.prototype.setAttribute = document.setAttribute;
Script.prototype.cloneNode = document.cloneNode;
Script.prototype.hasAttribute = document.hasAttribute;
Script.prototype.removeAttribute = document.removeAttribute;
Script.prototype.appendChild = document.appendChild;
Script.prototype.apch1$ = document.apch1$;
Script.prototype.apch2$ = document.apch2$;
Script.prototype.insertBefore = document.insertBefore;
Script.prototype.insbf$ = document.insbf$;
Script.prototype.hasChildNodes = document.hasChildNodes;
Script.prototype.removeChild = document.removeChild;
Script.prototype.replaceChild = document.replaceChild;

Object.defineProperty(Script.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Script.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Script.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Script.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

TextNode.prototype.getAttribute = document.getAttribute;
TextNode.prototype.setAttribute = document.setAttribute;
TextNode.prototype.cloneNode = document.cloneNode;
TextNode.prototype.hasAttribute = document.hasAttribute;
TextNode.prototype.removeAttribute = document.removeAttribute;
TextNode.prototype.appendChild = document.appendChild;
TextNode.prototype.apch1$ = document.apch1$;
TextNode.prototype.apch2$ = document.apch2$;
TextNode.prototype.insertBefore = document.insertBefore;
TextNode.prototype.insbf$ = document.insbf$;
TextNode.prototype.hasChildNodes = document.hasChildNodes;
TextNode.prototype.removeChild = document.removeChild;
TextNode.prototype.replaceChild = document.replaceChild;

Object.defineProperty(TextNode.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(TextNode.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(TextNode.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(TextNode.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

P.prototype.appendChild = document.appendChild;
P.prototype.apch1$ = document.apch1$;
P.prototype.apch2$ = document.apch2$;
P.prototype.getAttribute = document.getAttribute;
P.prototype.setAttribute = document.setAttribute;
P.prototype.hasAttribute = document.hasAttribute;
P.prototype.removeAttribute = document.removeAttribute;

P.prototype.insertBefore = document.insertBefore;
P.prototype.insbf$ = document.insbf$;
Object.defineProperty(P.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(P.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(P.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(P.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

P.prototype.hasChildNodes = document.hasChildNodes;
P.prototype.removeChild = document.removeChild;
P.prototype.replaceChild = document.replaceChild;
P.prototype.cloneNode = document.cloneNode;

Lister.prototype.appendChild = document.appendChild;
Lister.prototype.apch1$ = document.apch1$;
Lister.prototype.apch2$ = document.apch2$;
Lister.prototype.getAttribute = document.getAttribute;
Lister.prototype.setAttribute = document.setAttribute;
Lister.prototype.insertBefore = document.insertBefore;
Lister.prototype.insbf$ = document.insbf$;
Object.defineProperty(Lister.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Lister.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Lister.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Lister.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Lister.prototype.hasChildNodes = document.hasChildNodes;
Lister.prototype.removeChild = document.removeChild;
Lister.prototype.replaceChild = document.replaceChild;
Lister.prototype.cloneNode = document.cloneNode;
Lister.prototype.hasAttribute = document.hasAttribute;
Lister.prototype.removeAttribute = document.removeAttribute;

Listitem.prototype.appendChild = document.appendChild;
Listitem.prototype.apch1$ = document.apch1$;
Listitem.prototype.apch2$ = document.apch2$;
Listitem.prototype.getAttribute = document.getAttribute;
Listitem.prototype.setAttribute = document.setAttribute;
Listitem.prototype.insertBefore = document.insertBefore;
Listitem.prototype.insbf$ = document.insbf$;
Object.defineProperty(Listitem.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Listitem.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Listitem.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Listitem.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Listitem.prototype.hasChildNodes = document.hasChildNodes;
Listitem.prototype.removeChild = document.removeChild;
Listitem.prototype.replaceChild = document.replaceChild;
Listitem.prototype.cloneNode = document.cloneNode;
Listitem.prototype.hasAttribute = document.hasAttribute;
Listitem.prototype.removeAttribute = document.removeAttribute;

Table.prototype.appendChild = document.appendChild;
Table.prototype.apch1$ = document.apch1$;
Table.prototype.apch2$ = document.apch2$;
Table.prototype.getAttribute = document.getAttribute;
Table.prototype.setAttribute = document.setAttribute;
Table.prototype.insertBefore = document.insertBefore;
Table.prototype.insbf$ = document.insbf$;
Object.defineProperty(Table.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Table.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Table.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Table.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Table.prototype.hasChildNodes = document.hasChildNodes;
Table.prototype.removeChild = document.removeChild;
Table.prototype.replaceChild = document.replaceChild;
Table.prototype.cloneNode = document.cloneNode;
Table.prototype.hasAttribute = document.hasAttribute;
Table.prototype.removeAttribute = document.removeAttribute;

Tbody.prototype.appendChild = document.appendChild;
Tbody.prototype.apch1$ = document.apch1$;
Tbody.prototype.apch2$ = document.apch2$;
Tbody.prototype.getAttribute = document.getAttribute;
Tbody.prototype.setAttribute = document.setAttribute;
Tbody.prototype.insertBefore = document.insertBefore;
Tbody.prototype.insbf$ = document.insbf$;
Object.defineProperty(Tbody.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Tbody.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Tbody.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Tbody.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Tbody.prototype.hasChildNodes = document.hasChildNodes;
Tbody.prototype.removeChild = document.removeChild;
Tbody.prototype.replaceChild = document.replaceChild;
Tbody.prototype.cloneNode = document.cloneNode;
Tbody.prototype.hasAttribute = document.hasAttribute;
Tbody.prototype.removeAttribute = document.removeAttribute;

Trow.prototype.appendChild = document.appendChild;
Trow.prototype.apch1$ = document.apch1$;
Trow.prototype.apch2$ = document.apch2$;
Trow.prototype.getAttribute = document.getAttribute;
Trow.prototype.setAttribute = document.setAttribute;
Trow.prototype.insertBefore = document.insertBefore;
Trow.prototype.insbf$ = document.insbf$;
Object.defineProperty(Trow.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Trow.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Trow.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Trow.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Trow.prototype.hasChildNodes = document.hasChildNodes;
Trow.prototype.removeChild = document.removeChild;
Trow.prototype.replaceChild = document.replaceChild;
Trow.prototype.cloneNode = document.cloneNode;
Trow.prototype.hasAttribute = document.hasAttribute;
Trow.prototype.removeAttribute = document.removeAttribute;

Cell.prototype.appendChild = document.appendChild;
Cell.prototype.apch1$ = document.apch1$;
Cell.prototype.apch2$ = document.apch2$;
Cell.prototype.getAttribute = document.getAttribute;
Cell.prototype.setAttribute = document.setAttribute;
Cell.prototype.insertBefore = document.insertBefore;
Cell.prototype.insbf$ = document.insbf$;
Object.defineProperty(Cell.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Cell.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Cell.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Cell.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});


Cell.prototype.hasChildNodes = document.hasChildNodes;
Cell.prototype.removeChild = document.removeChild;
Cell.prototype.replaceChild = document.replaceChild;
Cell.prototype.cloneNode = document.cloneNode;
Cell.prototype.hasAttribute = document.hasAttribute;
Cell.prototype.removeAttribute = document.removeAttribute;

Span.prototype.appendChild = document.appendChild;
Span.prototype.apch1$ = document.apch1$;
Span.prototype.apch2$ = document.apch2$;
Span.prototype.getAttribute = document.getAttribute;
Span.prototype.setAttribute = document.setAttribute;
Span.prototype.insertBefore = document.insertBefore;
Span.prototype.insbf$ = document.insbf$;
Object.defineProperty(Span.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Span.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Span.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Span.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Span.prototype.hasChildNodes = document.hasChildNodes;
Span.prototype.removeChild = document.removeChild;
Span.prototype.replaceChild = document.replaceChild;
Span.prototype.cloneNode = document.cloneNode;
Span.prototype.hasAttribute = document.hasAttribute;
Span.prototype.removeAttribute = document.removeAttribute;

Image.prototype.appendChild = document.appendChild;
Image.prototype.apch1$ = document.apch1$;
Image.prototype.apch2$ = document.apch2$;
Image.prototype.getAttribute = document.getAttribute;
Image.prototype.setAttribute = document.setAttribute;
Image.prototype.insertBefore = document.insertBefore;
Image.prototype.insbf$ = document.insbf$;
Object.defineProperty(Image.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Image.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Image.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Image.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Image.prototype.hasChildNodes = document.hasChildNodes;
Image.prototype.removeChild = document.removeChild;
Image.prototype.replaceChild = document.replaceChild;
Image.prototype.cloneNode = document.cloneNode;
Image.prototype.hasAttribute = document.hasAttribute;
Image.prototype.removeAttribute = document.removeAttribute;

Frame.prototype.appendChild = document.appendChild;
Frame.prototype.apch1$ = document.apch1$;
Frame.prototype.apch2$ = document.apch2$;
Frame.prototype.getAttribute = document.getAttribute;
Frame.prototype.setAttribute = document.setAttribute;
Frame.prototype.insertBefore = document.insertBefore;
Frame.prototype.insbf$ = document.insbf$;
Object.defineProperty(Frame.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Frame.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Frame.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Frame.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Frame.prototype.hasChildNodes = document.hasChildNodes;
Frame.prototype.removeChild = document.removeChild;
Frame.prototype.replaceChild = document.replaceChild;
Frame.prototype.cloneNode = document.cloneNode;
Frame.prototype.hasAttribute = document.hasAttribute;
Frame.prototype.removeAttribute = document.removeAttribute;

Option.prototype.appendChild = document.appendChild;
Option.prototype.apch1$ = document.apch1$;
Option.prototype.apch2$ = document.apch2$;
Option.prototype.getAttribute = document.getAttribute;
Option.prototype.setAttribute = document.setAttribute;
Option.prototype.insertBefore = document.insertBefore;
Option.prototype.insbf$ = document.insbf$;
Object.defineProperty(Option.prototype, "firstChild", {
get: function() { return this.childNodes[0]; }
});
Object.defineProperty(Option.prototype, "lastChild", {
get: function() { return this.childNodes[this.childNodes.length-1]; }
});
Object.defineProperty(Option.prototype, "nextSibling", {
get: function() { return get_sibling(this,"next"); }
});
Object.defineProperty(Option.prototype, "previousSibling", {
get: function() { return get_sibling(this,"previous"); }
});

Option.prototype.hasChildNodes = document.hasChildNodes;
Option.prototype.removeChild = document.removeChild;
Option.prototype.replaceChild = document.replaceChild;
Option.prototype.cloneNode = document.cloneNode;
Option.prototype.hasAttribute = document.hasAttribute;
Option.prototype.removeAttribute = document.removeAttribute;

/* navigator; some parameters are filled in by the buildstartwindow script. */
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/mozjs";
/* not sure what product is about */
navigator.product = "mozjs";
navigator.productSub = "2.4";
navigator.vendor = "Karl Dahlke";
/* language is determined in C, at runtime, by $LANG */
navigator.javaEnabled = function() { return false; }
navigator.taintEnabled = function() { return false; }
navigator.cookieEnabled = true;
navigator.onLine = true;

/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
history.length = 1;
history.next = "";
history.previous = "";
history.back = function() { return null; }
history.forward = function() { return null; }
history.go = function() { return null; }
history.toString = function() {
 return "Sorry, edbrowse does not maintain a browsing history.";
} 

/* The web console, one argument, print based on debugLevel */
time$log$ = function(debug, level, obj) {
var today=new Date();
var h=today.getHours();
var m=today.getMinutes();
var s=today.getSeconds();
// add a zero in front of numbers<10
if(h < 10) h = "0" + h;
if(m < 10) m = "0" + m;
if(s < 10) s = "0" + s;
$logputs$(debug, "console " + level + " [" + h + ":" + m + ":" + s + "] " + obj);
}
console = new Object;
console.log = function(obj) { time$log$(3, "log", obj); }
console.info = function(obj) { time$log$(3, "info", obj); }
console.warn = function(obj) { time$log$(3, "warn", obj); }
console.error = function(obj) { time$log$(3, "error", obj); }

/* An ok (object keys) function for javascript/dom debugging. */
/* This is in concert with the jdb command in edbrowse. */
ok = Object.keys = Object.keys || (function () { 
		var hasOwnProperty = Object.prototype.hasOwnProperty, 
		hasDontEnumBug = !{toString:null}.propertyIsEnumerable("toString"),
		DontEnums = [ 
		'toString', 'toLocaleString', 'valueOf', 'hasOwnProperty', 
		'isPrototypeOf', 'propertyIsEnumerable', 'constructor' 
		], 
		DontEnumsLength = DontEnums.length; 
		return function (o) { 
		if (typeof o != "object" && typeof o != "function" || o === null) 
		throw new TypeError("Object.keys called on a non-object");
		var result = []; 
		for (var name in o) { 
		if (hasOwnProperty.call(o, name)) 
		result.push(name); 
		} 
		if (hasDontEnumBug) { 
		for (var i = 0; i < DontEnumsLength; i++) { 
		if (hasOwnProperty.call(o, DontEnums[i]))
		result.push(DontEnums[i]);
		}
		}
		return result; 
		}; 
		})(); 

// ------------ experimental below this point
// @author Originally implemented by Yehuda Katz
// And since then, from envjs, by Thatcher et al

var domparser;

XMLHttpRequest = function(){
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
};

// defined by the standard: http://www.w3.org/TR/XMLHttpRequest/#xmlhttprequest
// but not provided by Firefox.  Safari and others do define it.
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;

XMLHttpRequest.prototype = {
open: function(method, url, async, user, password){
this.readyState = 1;
this.async = false;
// Note: async is currently hardcoded to false
// In the implementation in envjs, the line here was:
// this.async = (async === false)?false:true;

this.method = method || "GET";
this.url = convert_url(url);
this.status = 0;
this.statusText = "";
this.onreadystatechange();
},
setRequestHeader: function(header, value){
this.headers[header] = value;
},
send: function(data, parsedoc/*non-standard*/){
var headerstring = "";
for (var item in this.headers) {
var v1=item;
var v2=this.headers[item];
headerstring+=v1;
headerstring+=": ";
headerstring+=v2;
headerstring+=",";
}

var entire_http_response =  document.fetchHTTP(this.url,this.method,headerstring,data);

var http_headers = entire_http_response.split("\r\n\r\n")[0];

var responsebody_array = entire_http_response.split("\r\n\r\n");
responsebody_array[0] = "";
var responsebody = responsebody_array.join("\r\n\r\n");
responsebody = responsebody.trim();

this.responseText = responsebody;
var hhc = http_headers.split("\r\n");
var i=0;
while (i < hhc.length) {
var value1 = hhc[i]+":";
var value2 = value1.split(":")[0];
var value3 = value1.split(":")[1];
this.responseHeaders[value2] = value3.trim();
i++;
}

try{
this.readyState = 4;
}catch(e){
}


if ((!this.aborted) && this.responseText.length > 0){
this.readyState = 4;
this.status = 200;
this.statusText = "OK";
this.onreadystatechange();
}

},
abort: function(){
this.aborted = true;
},
onreadystatechange: function(){
//Instance specific
},
getResponseHeader: function(header){
var rHeader, returnedHeaders;
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
returnedHeaders = [];
for (rHeader in this.responseHeaders) {
if (rHeader.match(new RegExp(header, "i"))) {
returnedHeaders.push(this.responseHeaders[rHeader]);
}
}

if (returnedHeaders.length){
return returnedHeaders.join(", ");
}
}
return null;
},
getAllResponseHeaders: function(){
var header, returnedHeaders = [];
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
for (header in this.responseHeaders) {
returnedHeaders.push( header + ": " + this.responseHeaders[header] );
}
}
return returnedHeaders.join("\r\n");
},
async: false,
readyState: 0,
responseText: "",
status: 0,
statusText: ""
};

convert_url = function(path, base) {

var new_url;
var sideprotocol =location.protocol;
var sidehost = location.host;
var lpl  = location.protocol.length;
if (lpl ==  0)
{
sideprotocol = "http:";
sideprotocol.length = 5;
lpl = 5;
}
if (sidehost == "")
{
sidehost = location.href;
}

var protocol1= path.substring(0,lpl);
if (protocol1 == sideprotocol)
{
new_url = path;
// ok - it is fully qualified already
} else {

sidehost_last = sidehost.substring(sidehost.length-1,sidehost.length);
path_first = path.substring(0,1);
if (sidehost_last !== '/' && path_first !== '/')
{
new_url = sideprotocol + '//' + sidehost + '/' + path;
} else {
new_url = sideprotocol + '//' + sidehost + path;
}
}
return new_url;
}

document.defaultView = function()
{
return this.style;
}

document.defaultView.getComputedStyle = function()
{
        obj = new CSSStyleDeclaration;
        obj.element = document;
        obj.style = document.style;
        return obj;
}

getComputedStyle = function(n)
{
        obj = new CSSStyleDeclaration;
        obj.element = this;
        obj.style = new Array;
        obj.style.push({n:obj.style[n]});
        return obj;
}

CSSStyleDeclaration = function(){
        this.element = null;
        this.style = null;
};

CSSStyleDeclaration.prototype = {
getPropertyValue: function (n)
        {
                if (this.style[n] == undefined)
                {
                        this.style[n] = 0;
                        return 0;
                } else {
                        return this.style[n];
                }
        }
}
