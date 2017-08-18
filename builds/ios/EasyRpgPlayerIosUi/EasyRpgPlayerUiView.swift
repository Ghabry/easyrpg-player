//
//  EasyRpgPlayerUiView.swift
//  SDLTest
//
//  Created by EasyRPG on 17.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

import Foundation

import UIKit

import SDL2

@objc(EasyRpgPlayerViewController)
class EasyRpgPlayerViewController: UIViewController {
    
    var circleView : UIView?
    var gesture : UITapGestureRecognizer?
    var gesture2 : UIPanGestureRecognizer?
    
    public func setupUi() {
        circleView = UIView(frame:CGRect(x: 200, y: 500, width: 50, height: 50))
        circleView?.alpha = 1
        circleView?.layer.cornerRadius = 25
        circleView?.backgroundColor = UIColor.blue
        
        var label = UILabel(frame: CGRect(x: 0, y: 0, width: 50, height: 50))
        label.text = "A"
        label.textColor = UIColor.black
        label.frame.origin = (circleView?.frame.origin)!
        //label.center = (circleView?.center)!
        circleView?.addSubview(label)
        
        self.view.addSubview(circleView!)
        
        gesture = UITapGestureRecognizer(target: self, action: #selector(someAction))
        circleView?.addGestureRecognizer(gesture!)
        
        gesture2 = UIPanGestureRecognizer(target: self, action: #selector(someAction2))
        circleView?.addGestureRecognizer(gesture2!)
        /*let circleLayer = CAShapeLayer()
         circleLayer.path = UIBezierPath(ovalIn:CGRect(x: 50, y: 50, width: 100 , height: 100)).cgPath
         
         self.view.layer.addSublayer(circleLayer)
         */
    }
    
    func someAction(_ sender:UITapGestureRecognizer){
        if sender.state != .ended {
            //s.backgroundColor = UIColor.red
            var evnt = SDL2.SDL_Event()
        
            evnt.type = SDL2.SDL_KEYDOWN.rawValue
            evnt.key.keysym.scancode = SDL_Scancode(rawValue: SDL2.SDL_SCANCODE_RETURN.rawValue)
            print(evnt.key.keysym.scancode)
            print(SDL2.SDL_PushEvent(&evnt))
            
            /*evnt.type = SDL2.SDL_KEYUP.rawValue
            evnt.key.keysym.scancode = SDL_Scancode(rawValue: SDL2.SDL_SCANCODE_RETURN.rawValue)
            print(SDL2.SDL_PushEvent(&evnt))*/
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

class EasyRpgPlayerUiView: UIView {
    @IBOutlet weak var button: UIButton!
    /*override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
    }*/

    @IBAction func buttonPressed(_ sender: Any) {
        print("Hallo Welt")
    }
    
}
