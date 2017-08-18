//
//  EditLayoutView.swift
//  SDLTest
//
//  Created by EasyRPG on 17.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

import Foundation

import UIKit

class EditLayoutView : UIViewController {
    var circleView : UIView?
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        circleView = UIView(frame:CGRect(x: 200, y: 20, width: 100, height: 100))
        circleView?.alpha = 1
        circleView?.layer.cornerRadius = 50
        circleView?.backgroundColor = UIColor.blue
        
        self.view.addSubview(circleView!)
        
        let gesture = UITapGestureRecognizer(target: self, action: #selector(someAction))
        circleView?.addGestureRecognizer(gesture)
        
        let gesture2 = UIPanGestureRecognizer(target: self, action: #selector(someAction2))
        circleView?.addGestureRecognizer(gesture2)
        /*let circleLayer = CAShapeLayer()
        circleLayer.path = UIBezierPath(ovalIn:CGRect(x: 50, y: 50, width: 100 , height: 100)).cgPath
        
        self.view.layer.addSublayer(circleLayer)
        */
    }
    
    func someAction(_ sender:UITapGestureRecognizer){
        if let s = sender.view {
            s.backgroundColor = UIColor.red
        } else {
            
        }
    }
    
    func adjustAnchorPoint(gestureRecognizer : UIGestureRecognizer) {
        if gestureRecognizer.state == .began {
            let view = gestureRecognizer.view
            let locationInView = gestureRecognizer.location(in: view)
            let locationInSuperview = gestureRecognizer.location(in: view?.superview)
            
            // Move the anchor point to the touch point and change the position of the view
            view?.layer.anchorPoint = CGPoint(x: (locationInView.x / (view?.bounds.size.width)!),
                                              y: (locationInView.y / (view?.bounds.size.height)!))
            view?.center = locationInSuperview
        }
    }
    
    func someAction2(_ sender:UIPanGestureRecognizer){
        // Move the anchor point of the view's layer to the touch point
        // so that moving the view becomes simpler.
        let piece = sender.view
        self.adjustAnchorPoint(gestureRecognizer: sender)
        
        if sender.state == .began || sender.state == .changed {
            // Get the distance moved since the last call to this method.
            let translation = sender.translation(in: piece?.superview)
            
            // Set the translation point to zero so that the translation distance
            // is only the change since the last call to this method.
            piece?.center = CGPoint(x: ((piece?.center.x)! + translation.x),
                                    y: ((piece?.center.y)! + translation.y))
            sender.setTranslation(CGPoint.zero, in: piece?.superview)
        }
    }
}
