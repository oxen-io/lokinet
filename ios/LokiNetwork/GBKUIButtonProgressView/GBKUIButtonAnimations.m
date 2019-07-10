//
//  GBKUIButtonAnimations.m
//  GBKUIComponentLibraryExample
//
//  Created by James Hung on 9/9/15.
//  Copyright (c) 2015 James Hung. All rights reserved.
//

#import "GBKUIButtonAnimations.h"

static const CGFloat GBKUIButtonAnimationBaseScaleFactor = 0.98;
static const CGFloat GBKUIButtonAnimationAlpha = 0.8;

@implementation GBKUIButtonAnimations

+ (CAAnimation *)touchDownAnimationWithXScaleFactor:(CGFloat)xScaleFactor yScaleFactor:(CGFloat)yScaleFactor {
    CABasicAnimation *xScaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform.scale.x"];
    xScaleAnimation.fromValue = @1;
    xScaleAnimation.toValue = @(xScaleFactor);
    xScaleAnimation.duration = 0.15;
    
    CABasicAnimation *yScaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform.scale.y"];
    yScaleAnimation.fromValue = @1;
    yScaleAnimation.toValue = @(yScaleFactor);
    yScaleAnimation.duration = 0.15;
    
    CABasicAnimation *alphaAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    alphaAnimation.fromValue = @1;
    alphaAnimation.toValue = @0.8;
    alphaAnimation.duration = 0.15;
    
    xScaleAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    yScaleAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    alphaAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    
    CAAnimationGroup *touchDownAnimation = [[CAAnimationGroup alloc] init];
    touchDownAnimation.animations = @[xScaleAnimation, yScaleAnimation, alphaAnimation];
    touchDownAnimation.duration = 0.15;
    return touchDownAnimation;
}

+ (CAAnimation *)touchUpAnimationWithXScaleFactor:(CGFloat)xScaleFactor yScaleFactor:(CGFloat)yScaleFactor {
    CABasicAnimation *xScaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform.scale.x"];
    xScaleAnimation.fromValue = @(xScaleFactor);
    xScaleAnimation.toValue = @1;
    xScaleAnimation.duration = 0.5;
    
    CABasicAnimation *yScaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform.scale.y"];
    yScaleAnimation.fromValue = @(yScaleFactor);
    yScaleAnimation.toValue = @1;
    yScaleAnimation.duration = 0.5;
    
    CABasicAnimation *alphaAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    alphaAnimation.fromValue = @0.8;
    alphaAnimation.toValue = @1;
    alphaAnimation.duration = 0.5;
    
    xScaleAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    yScaleAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    alphaAnimation.timingFunction = [CAMediaTimingFunction functionWithControlPoints:0.34:1.61:0.7:1.0];
    
    CAAnimationGroup *touchUpAnimation = [[CAAnimationGroup alloc] init];
    touchUpAnimation.animations = @[xScaleAnimation, yScaleAnimation, alphaAnimation];
    touchUpAnimation.duration = 0.5;
    return touchUpAnimation;
}

+ (void)applyTouchDownAnimationForView:(UIView *)view {
    [view.layer removeAllAnimations];
    
    CGFloat xScaleFactor = floorf(GBKUIButtonAnimationBaseScaleFactor * CGRectGetWidth(view.frame))/CGRectGetWidth(view.frame);
    
    CGFloat yScaleFactor = floorf(GBKUIButtonAnimationBaseScaleFactor * CGRectGetHeight(view.frame))/CGRectGetHeight(view.frame);
    
    CAAnimation *animation = [self touchDownAnimationWithXScaleFactor:xScaleFactor yScaleFactor:yScaleFactor];
    [view.layer addAnimation:animation forKey:@"GBKUIButtonTouchDownAnimation"];
    view.transform = CGAffineTransformMakeScale(xScaleFactor, yScaleFactor);
    view.alpha = GBKUIButtonAnimationAlpha;
}

+ (void)applyTouchUpAnimationForView:(UIView *)view {
    [view.layer removeAllAnimations];
    
    CGFloat xScaleFactor = floorf(GBKUIButtonAnimationBaseScaleFactor * CGRectGetWidth(view.frame))/CGRectGetWidth(view.frame);
    
    CGFloat yScaleFactor = floorf(GBKUIButtonAnimationBaseScaleFactor * CGRectGetHeight(view.frame))/CGRectGetHeight(view.frame);
    
    CAAnimation *animation = [self touchUpAnimationWithXScaleFactor:xScaleFactor yScaleFactor:yScaleFactor];
    [view.layer addAnimation:animation forKey:@"GBKUIButtonTouchUpAnimation"];
    view.transform = CGAffineTransformMakeScale(1.0, 1.0);
    view.alpha = 1.0;
}



@end
