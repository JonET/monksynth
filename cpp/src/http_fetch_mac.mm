#include "http_fetch.h"

#import <Foundation/Foundation.h>

namespace MonkSynth {

HttpResult httpGet(const std::string &url, std::size_t maxBytes) {
    @autoreleasepool {
        __block HttpResult result;

        NSURL *nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        if (!nsurl) {
            result.error = "Bad URL";
            return result;
        }

        // Ephemeral: no cookies or disk cache in the host's container.
        NSURLSessionConfiguration *config =
            [NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.timeoutIntervalForRequest = 20;
        config.timeoutIntervalForResource = 120;
        config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        NSURLSession *session = [NSURLSession sessionWithConfiguration:config];

        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        NSURLSessionDataTask *task = [session
              dataTaskWithURL:nsurl
            completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                if (error) {
                    const char *msg = error.localizedDescription.UTF8String;
                    result.error = msg ? msg : "Network error";
                } else {
                    NSInteger status = 0;
                    if ([response isKindOfClass:[NSHTTPURLResponse class]])
                        status = ((NSHTTPURLResponse *)response).statusCode;
                    if (status != 200) {
                        result.error = "HTTP " + std::to_string(static_cast<long>(status));
                    } else if (data.length > maxBytes) {
                        result.error = "Response too large";
                    } else {
                        result.body.assign(static_cast<const char *>(data.bytes), data.length);
                        result.ok = true;
                    }
                }
                dispatch_semaphore_signal(done);
            }];
        [task resume];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        [session finishTasksAndInvalidate];
#if !__has_feature(objc_arc)
        dispatch_release(done);
#endif
        return result;
    }
}

} // namespace MonkSynth
