//
//  ViewController.swift
//  SDLTest
//
//  Created by EasyRPG on 16.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

import UIKit

class GameBrowserViewController: UITableViewController {
    @IBOutlet weak var myButton: UIButton!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view, typically from a nib.
    }

    @IBAction func myButtonClicked(_ sender: Any) {
        start_sdl()
    }
}

