//
//  GBKUIProgressButton.h
//  Pods
//
//  Created by Peter Lada.
//
//

#import <UIKit/UIKit.h>

@interface GBKUIButtonProgressView : UIView

@property (strong, nonatomic) NSString *completeTitle;
@property (strong, nonatomic) NSString *initialTitle;
@property (strong, nonatomic) UIFont *font;
@property (assign, nonatomic, getter=isDisabled) BOOL disabled;
@property (assign, nonatomic, readonly) CGFloat progress;
@property (assign, nonatomic, readonly) BOOL isProgressing;
@property (assign, nonatomic, readonly) BOOL isComplete;

- (void)startProgressing;
- (void)completeProgressing;
- (void)reset;
- (void)setProgress:(CGFloat)progress animated:(BOOL)animated;
- (void)setProgress:(CGFloat)progress animated:(BOOL)animated withCompletion:(void(^)())completion;
- (void)addTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)controlEvents;


@end
