#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"

struct DiaryDetail {
  bool ok = false;
  String message;
  String id;
  String title;
  String content;
  String category;
  String date;
};

// POST /user/login — saves token (+ email) on success. Captures X-Access-Token if present.
bool diaryLogin(const char *email, const char *password, String &err);

// GET /diary/detail?diaryId= — uses saved token; persists renewed X-Access-Token.
bool diaryFetchById(const char *diary_id, DiaryDetail &out);

// Ensure token exists; if empty returns false (need web login).
bool diaryHasToken();

// Draw diary as a note page (not todo checkbox layout).
void diaryDrawPage(EpdSsd2683 &epd, const DiaryDetail &diary, const char *status_line = nullptr);

// Connect (if needed), fetch configured diary id, draw as diary app.
bool diaryRefreshAndDraw(EpdSsd2683 &epd);
