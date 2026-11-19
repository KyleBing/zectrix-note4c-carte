#pragma once

// Album tab HTML body (dither JS inline). Caller wraps with sendHtml.
void webAlbumAppendPage(class String &body);
void webAlbumRegisterRoutes(class WebServer &server);
