#pragma once

class WebServer;
class String;

// Album tab HTML body (no inline script). Caller wraps with sendHtml / streams JS.
void webAlbumAppendPage(String &body);
// Stream the large album <script> without building a giant Arduino String.
void webAlbumSendScript(WebServer &server);
void webAlbumRegisterRoutes(WebServer &server);
