/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkCodec.h"
#include "SkStream.h"

#include "Resources.h"
#include "Test.h"

#include <initializer_list>
#include <vector>

DEF_TEST(Codec_frames, r) {
    static const struct {
        const char*         fName;
        size_t              fFrameCount;
        // One less than fFramecount, since the first frame is always
        // independent.
        std::vector<size_t> fRequiredFrames;
        // The size of this one should match fFrameCount for animated, empty
        // otherwise.
        std::vector<size_t> fDurations;
    } gRecs[] = {
        { "box.gif", 1, {}, {} },
        { "color_wheel.gif", 1, {}, {} },
        { "test640x479.gif", 4, { 0, 1, 2 }, { 200, 200, 200, 200 } },
        { "colorTables.gif", 2, { 0 }, { 1000, 1000 } },

        { "arrow.png",  1, {}, {} },
        { "google_chrome.ico", 1, {}, {} },
        { "brickwork-texture.jpg", 1, {}, {} },
#if defined(SK_CODEC_DECODES_RAW) && (!defined(_WIN32))
        { "dng_with_preview.dng", 1, {}, {} },
#endif
        { "mandrill.wbmp", 1, {}, {} },
        { "randPixels.bmp", 1, {}, {} },
        { "yellow_rose.webp", 1, {}, {} },
    };

    for (auto rec : gRecs) {
        std::unique_ptr<SkStream> stream(GetResourceAsStream(rec.fName));
        if (!stream) {
            // Useful error statement, but sometimes people run tests without
            // resources, and they do not want to see these messages.
            //ERRORF(r, "Missing resources? Could not find '%s'", rec.fName);
            continue;
        }

        std::unique_ptr<SkCodec> codec(SkCodec::NewFromStream(stream.release()));
        if (!codec) {
            ERRORF(r, "Failed to create an SkCodec from '%s'", rec.fName);
            continue;
        }

        const size_t expected = rec.fFrameCount;
        const auto frameInfos = codec->getFrameInfo();
        // getFrameInfo returns empty set for non-animated.
        const size_t frameCount = frameInfos.size() == 0 ? 1 : frameInfos.size();
        if (frameCount != expected) {
            ERRORF(r, "'%s' expected frame count: %i\tactual: %i", rec.fName, expected, frameCount);
            continue;
        }

        if (rec.fRequiredFrames.size() + 1 != expected) {
            ERRORF(r, "'%s' has wrong number entries in fRequiredFrames; expected: %i\tactual: %i",
                   rec.fName, expected, rec.fRequiredFrames.size() + 1);
            continue;
        }

        if (1 == frameCount) {
            continue;
        }

        // From here on, we are only concerned with animated images.
        REPORTER_ASSERT(r, frameInfos[0].fRequiredFrame == SkCodec::kNone);
        for (size_t i = 1; i < frameCount; i++) {
            REPORTER_ASSERT(r, rec.fRequiredFrames[i-1] == frameInfos[i].fRequiredFrame);
        }

        // Compare decoding in two ways:
        // 1. Provide the frame that a frame depends on, so the codec just has to blend.
        //    (in the array cachedFrames)
        // 2. Do not provide the frame that a frame depends on, so the codec has to decode all the
        //    way back to a key-frame. (in a local variable uncachedFrame)
        // The two should look the same.
        std::vector<SkBitmap> cachedFrames(frameCount);
        const auto& info = codec->getInfo().makeColorType(kN32_SkColorType);

        auto decode = [&](SkBitmap* bm, bool cached, size_t index) {
            bm->allocPixels(info);
            if (cached) {
                // First copy the pixels from the cached frame
                const size_t requiredFrame = frameInfos[index].fRequiredFrame;
                if (requiredFrame != SkCodec::kNone) {
                    const bool success = cachedFrames[requiredFrame].copyTo(bm);
                    REPORTER_ASSERT(r, success);
                }
            }
            SkCodec::Options opts;
            opts.fFrameIndex = index;
            opts.fHasPriorFrame = cached;
            const SkCodec::Result result = codec->getPixels(info, bm->getPixels(), bm->rowBytes(),
                                                            &opts, nullptr, nullptr);
            REPORTER_ASSERT(r, result == SkCodec::kSuccess);
        };

        for (size_t i = 0; i < frameCount; i++) {
            SkBitmap& cachedFrame = cachedFrames[i];
            decode(&cachedFrame, true, i);
            SkBitmap uncachedFrame;
            decode(&uncachedFrame, false, i);

            // Now verify they're equal.
            const size_t rowLen = info.bytesPerPixel() * info.width();
            for (int y = 0; y < info.height(); y++) {
                const void* cachedAddr = cachedFrame.getAddr(0, y);
                SkASSERT(cachedAddr != nullptr);
                const void* uncachedAddr = uncachedFrame.getAddr(0, y);
                SkASSERT(uncachedAddr != nullptr);
                const bool lineMatches = memcmp(cachedAddr, uncachedAddr, rowLen) == 0;
                if (!lineMatches) {
                    ERRORF(r, "%s's frame %i is different depending on caching!", rec.fName, i);
                    break;
                }
            }
        }

        if (rec.fDurations.size() != expected) {
            ERRORF(r, "'%s' has wrong number entries in fDurations; expected: %i\tactual: %i",
                   rec.fName, expected, rec.fDurations.size());
            continue;
        }

        for (size_t i = 0; i < frameCount; i++) {
            REPORTER_ASSERT(r, rec.fDurations[i] == frameInfos[i].fDuration);
        }
    }
}
