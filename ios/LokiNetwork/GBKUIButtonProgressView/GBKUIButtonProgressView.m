//
//  GBKUIButtonProgress.h
//  Pods
//
//  Created by Peter Lada
//
//

#define DEGREES_TO_RADIANS(angle) ((angle) / 180.0 * M_PI)

#import "GBKUIButtonProgressView.h"
#import "GBKUIButtonAnimations.h"

typedef NS_ENUM(NSInteger, GBKUIButtonProgressState) {
    GBKUIButtonProgressInitial,
    GBKUIButtonProgressShrinking,
    GBKUIButtonProgressProgressing,
    GBKUIButtonProgressExpandingToComplete,
    GBKUIButtonProgressExpandingToInitial,
    GBKUIButtonProgressCompleted
};

@interface GBKUIButtonProgressView()

@property (weak, nonatomic) IBOutlet UILabel *titleLabel;
@property (weak, nonatomic) IBOutlet UIView *borderView;
@property (weak, nonatomic) IBOutlet UIButton *button;
@property (weak, nonatomic) IBOutlet UIImageView *pause;

@property (strong, nonatomic) UIImageView *arcContainer;
@property (strong, nonatomic) CAShapeLayer *arc;

@property (nonatomic) float expandDuration;

// initial -> shrinking -> progressing -> expanding -> completed
@property (assign, nonatomic) GBKUIButtonProgressState state;

@property (weak, nonatomic) IBOutlet NSLayoutConstraint *widthContraint;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *labelLeadingContstraint;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *labelTrailingContstraint;

@end

@implementation GBKUIButtonProgressView

- (instancetype)initWithCoder:(NSCoder *)aDecoder {
    self = [super initWithCoder:aDecoder];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)awakeFromNib {
    [super awakeFromNib];
    [self setupView];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.frame = frame;
        [self commonInit];
        [self setupView];
    }
    
    return self;
}

- (void)commonInit {
    UIView *view = [[[NSBundle bundleForClass:[self class]] loadNibNamed:NSStringFromClass([self class]) owner:self options:nil] firstObject];
    [view setTranslatesAutoresizingMaskIntoConstraints:NO];
    
    NSLayoutConstraint *topConstraint = [NSLayoutConstraint constraintWithItem:view attribute:NSLayoutAttributeTop relatedBy:NSLayoutRelationEqual toItem:self attribute:NSLayoutAttributeTop multiplier:1.0 constant:0];
    NSLayoutConstraint *bottomConstraint = [NSLayoutConstraint constraintWithItem:view attribute:NSLayoutAttributeBottom relatedBy:NSLayoutRelationEqual toItem:self attribute:NSLayoutAttributeBottom multiplier:1.0 constant:0];
    NSLayoutConstraint *leftConstraint = [NSLayoutConstraint constraintWithItem:view attribute:NSLayoutAttributeLeft relatedBy:NSLayoutRelationEqual toItem:self attribute:NSLayoutAttributeLeft multiplier:1.0 constant:0];
    NSLayoutConstraint *rightConstraint = [NSLayoutConstraint constraintWithItem:view attribute:NSLayoutAttributeRight relatedBy:NSLayoutRelationEqual toItem:self attribute:NSLayoutAttributeRight multiplier:1.0 constant:0];
    
    view.backgroundColor = [UIColor clearColor];
    [self addSubview:view];
    [self addConstraints:@[topConstraint, bottomConstraint, leftConstraint, rightConstraint]];
}

- (void)setupView {
    self.clipsToBounds = YES;
    self.backgroundColor = [UIColor clearColor];
    self.borderView.backgroundColor = [UIColor clearColor];
    self.borderView.layer.borderColor = self.tintColor.CGColor;
    self.borderView.layer.cornerRadius = 2.5;
    self.borderView.layer.borderWidth = 1;
    self.titleLabel.font = [UIFont systemFontOfSize:15.0];
    self.titleLabel.textColor = self.tintColor;
    self.pause.tintColor = self.tintColor;
    
    self.state = GBKUIButtonProgressInitial;
    [self setProgress:0 animated:NO];
    self.expandDuration = 0.25;
    [self createPauseButton];
}

- (void) animateToCircleWithCompletion:(void(^)(BOOL))completion {
    float diameter = self.frame.size.height;
    self.widthContraint.constant = diameter;
    self.widthContraint.priority = 999;
    
    [UIView animateWithDuration:self.expandDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseInOut|UIViewAnimationOptionBeginFromCurrentState
                     animations:^(void) {
                         [self layoutIfNeeded];
                         self.titleLabel.alpha = 0;
                     }
                     completion:^(BOOL finished) {
                         [self createArcView];
                     }];
    
    [UIView animateWithDuration:self.expandDuration
                          delay:self.expandDuration+0.15
                        options:UIViewAnimationOptionCurveEaseInOut|UIViewAnimationOptionBeginFromCurrentState
                     animations:^(void) {
                         self.pause.alpha = 1;
                         self.pause.transform = CGAffineTransformMakeScale(1, 1);
                     }
                     completion:completion];
    
    [self animateCornerRadiusTo:diameter/2];
}

- (void) animateCornerRadiusTo:(float)radius {
    CABasicAnimation *animation = [CABasicAnimation animationWithKeyPath:@"cornerRadius"];
    animation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
    animation.fromValue = [NSNumber numberWithFloat:self.borderView.layer.cornerRadius];
    animation.toValue = [NSNumber numberWithFloat:radius];
    animation.duration = self.expandDuration;
    [self.borderView.layer addAnimation:animation forKey:@"cornerRadius"];
    [self.borderView.layer setCornerRadius:radius];
}

- (void) animateToButtonWithCompletion:(void(^)())completion {
    [UIView animateWithDuration:self.expandDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseInOut|UIViewAnimationOptionBeginFromCurrentState
                     animations:^(void){
                         self.arcContainer.alpha = 0;
                         self.pause.alpha = 0;
                         self.pause.transform = CGAffineTransformMakeScale(.7, .7);
                     }
                     completion:^(BOOL completed) {
                         [self.arcContainer removeFromSuperview];
                         self.arcContainer = nil;
                     }];
    
    self.widthContraint.priority = 1;
    [UIView animateWithDuration:self.expandDuration
                          delay:MAX(self.expandDuration-0.5, 0)
                        options:UIViewAnimationOptionCurveEaseInOut|UIViewAnimationOptionBeginFromCurrentState
                     animations:^(void){
                         self.titleLabel.alpha = 1;
                         [self layoutIfNeeded];
                     }
                     completion:^(BOOL completed) {
                         if(completion) {
                             completion();
                         }
                     }];
    
    [self animateCornerRadiusTo:2.5];
}

- (void) createArcView {
    self.arcContainer = [[UIImageView alloc] initWithFrame:CGRectMake(0, 0, self.borderView.frame.size.width, self.borderView.frame.size.width)];
    self.arcContainer.autoresizingMask = (UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin);
    [self.borderView addSubview:self.arcContainer];
    
    CGFloat strokeWidth = 3;
    CGFloat radius = (self.arcContainer.frame.size.width-strokeWidth) / 2;
    // draw arc
    self.arc = [CAShapeLayer layer];
    self.arc.frame = CGRectMake(0, 0, radius*2, radius*2);
    UIBezierPath *path = [UIBezierPath bezierPathWithArcCenter:self.arcContainer.center radius:radius-0.5 startAngle:DEGREES_TO_RADIANS(270) endAngle:DEGREES_TO_RADIANS(269) clockwise:YES];
    self.arc.path = path.CGPath;
    self.arc.fillColor = [UIColor clearColor].CGColor;
    self.arc.strokeColor = self.tintColor.CGColor;
    self.arc.lineWidth = strokeWidth;
    
    self.arc.strokeStart = 0;
    self.arc.strokeEnd = 0;
    
    [self.arcContainer.layer addSublayer:self.arc];
    
}

- (void) createPauseButton {
    UIImage *pauseImage = [[UIImage imageNamed:@"ic_control_stop"] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    self.pause.contentMode = UIViewContentModeCenter;
    self.pause.alpha = 0;
    self.pause.image = pauseImage;
    self.pause.transform = CGAffineTransformMakeScale(.5, .5);
}

#pragma mark - Public Methods

- (void) setInitialTitle:(NSString*)title {
    if(self.state == GBKUIButtonProgressInitial) {
        self.titleLabel.text = title.uppercaseString;
    }
    _initialTitle = title;
}

-(void)setCompleteTitle:(NSString *)completeTitle {
    if(self.state == GBKUIButtonProgressCompleted) {
        self.titleLabel.text = completeTitle.uppercaseString;
    }
    _completeTitle = completeTitle;
}

- (void)setFont:(UIFont *)font
{
    self.titleLabel.font = font;
}

- (UIFont *)font
{
    return self.titleLabel.font;
}

- (void)setProgress:(CGFloat)progress animated:(BOOL)animated {
    [self setProgress:progress animated:animated withCompletion:nil];
}

- (void)setProgress:(CGFloat)progress animated:(BOOL)animated withCompletion:(void(^)())completion {
    CGFloat previousProgress = _progress;
    _progress = progress;
    
    if(!animated || self.state != GBKUIButtonProgressProgressing) {
        if(completion != nil) {
            completion();
        }
        return;
    }
    
    [CATransaction begin];
    CABasicAnimation *animation = [CABasicAnimation animationWithKeyPath:@"strokeEnd"];
    animation.duration = 0.15;
    
    if ([[self.arc animationKeys] count] > 0) {
        animation.fromValue = @([self.arc.presentationLayer strokeEnd]);
        self.arc.strokeEnd = [self.arc.presentationLayer strokeEnd];
        [self.arc removeAllAnimations];
    } else {
        animation.fromValue = @(previousProgress);
    }
    
    animation.toValue = @(MAX(progress, 0.05));
    animation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionDefault];
    animation.fillMode = kCAFillModeBoth; // keep to value after finishing
    animation.removedOnCompletion = NO; // don't remove after finishing
    
    [CATransaction setCompletionBlock:completion];
    
    [self.arc addAnimation:animation forKey:@"strokeEnd"];
    [CATransaction commit];
}

- (void) startProgressing {
    if (self.state != GBKUIButtonProgressInitial) {
        return;
    }
    
    // shrink animation time
    self.state = GBKUIButtonProgressShrinking;
    __weak typeof(self) weakSelf = self;
    
    [self animateToCircleWithCompletion:^(BOOL finished) {
        
        // if the state was changed, we don't want to do anything
        if(self.state != GBKUIButtonProgressShrinking) {
            return;
        }
        // now we're in the expanding state
        self.state = GBKUIButtonProgressProgressing;
        
        // animate the progress from 0 to it's current position
        CGFloat currentProgress = weakSelf.progress;
        [weakSelf setProgress:0 animated:NO];
        [weakSelf setProgress:currentProgress animated:YES];
    }];
}

- (void) completeProgressing {
    [self setProgress:1 animated:NO];
    // if the state is initial, the we just change the title and we're complete
    if (self.state == GBKUIButtonProgressInitial) {
        self.titleLabel.text = self.completeTitle.uppercaseString;
        self.state = GBKUIButtonProgressCompleted;
        return;
    }
    
    // if the state is currently shrinking,
    if(self.state == GBKUIButtonProgressShrinking || self.state == GBKUIButtonProgressProgressing) {
        self.titleLabel.text = self.completeTitle.uppercaseString;
        self.state = GBKUIButtonProgressExpandingToComplete;
        self.disabled = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self animateToButtonWithCompletion:^{
                if(self.state == GBKUIButtonProgressExpandingToComplete) {
                    self.state = GBKUIButtonProgressCompleted;
                    self.disabled = NO;
                }
            }];
        });
        return;
    }
}

- (void)reset {
    [self setProgress:0 animated:NO];
    if (self.state == GBKUIButtonProgressCompleted) {
        self.titleLabel.text = self.initialTitle.uppercaseString;
        self.state = GBKUIButtonProgressInitial;
        return;
    }
    
    // if the state is currently shrinking,
    if(self.state == GBKUIButtonProgressShrinking || self.state == GBKUIButtonProgressProgressing) {
        self.titleLabel.text = self.initialTitle.uppercaseString;
        self.state = GBKUIButtonProgressExpandingToInitial;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self animateToButtonWithCompletion:^{
                if(self.state == GBKUIButtonProgressExpandingToInitial) {
                    self.state = GBKUIButtonProgressInitial;
                }
            }];
        });
        return;
    }
}

- (void)addTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)controlEvents {
    [self.button addTarget:target action:action forControlEvents:controlEvents];
}

- (void)setDisabled:(BOOL)disabled {
    _disabled = disabled;
    self.button.enabled = !disabled;
    self.alpha = disabled ? 0.45 : 1.0;
}


-(void)setState:(GBKUIButtonProgressState)state {
    _state = state;
    self.button.enabled = !self.disabled || (state != GBKUIButtonProgressExpandingToInitial && state != GBKUIButtonProgressShrinking && state != GBKUIButtonProgressExpandingToComplete);
}

-(void)setTintColor:(UIColor *)tintColor {
    [super setTintColor:tintColor];
    self.borderView.layer.borderColor = tintColor.CGColor;
    self.titleLabel.textColor = tintColor;
    self.pause.tintColor = tintColor;
}

-(BOOL)isProgressing {
    return self.state == GBKUIButtonProgressProgressing || self.state == GBKUIButtonProgressShrinking;
}

-(BOOL)isComplete {
    return self.state == GBKUIButtonProgressCompleted || self.state == GBKUIButtonProgressExpandingToComplete;
}

#pragma mark - Event Handling Notifications

- (IBAction)eventTouchUp:(id)sender {
    [self animateTouchUp];
}

- (IBAction)eventTouchDown:(id)sender {
    [self animateTouchDown];
}

- (IBAction)eventTouchDragExit:(id)sender {
    [self animateTouchUp];
}

#pragma mark - Animation Helper Methods

- (void)animateTouchUp {
    [GBKUIButtonAnimations applyTouchUpAnimationForView:self];
}

- (void)animateTouchDown {
    [GBKUIButtonAnimations applyTouchDownAnimationForView:self];
}

@end
