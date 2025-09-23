/*
	This script has a number of important general purpose classes and public procedures, mostly involving GUI elements.
*/
(version 2)
(include "sci.sh")
(exports
    0 MouseStillDown
    1 GetNumber
)
(use "Main")
(use "Print")
(use "System")
(script 255)


/*
	Determines if there is a mouse release event.
	
	:returns: FALSE if there is a mouse release event, TRUE otherwise.
*/
(procedure public (MouseStillDown)
    (var newEvent, temp1)
    = newEvent (Event:new())
    = temp1 (<> (send newEvent:type) evMOUSERELEASE)
    (send newEvent:dispose())
    return temp1
)

/**
	Shows an edit box and gets a number from the user.
	
	.. function:: GetNumber(title [defaultNumber])
	
		:param string title: A title for the dialog.
		:param defaultNumber: An optional default number.
	
		:returns: A number, or -1 if the user cancelled.
 */
(procedure public (GetNumber title defaultNumber)
    (var temp0[40])
    = temp0 0
    (if (> paramTotal 1)
        Format(@temp0 "%d" defaultNumber)
    )
    return 
        (if (GetInput(@temp0 5 title))
            ReadNumber(@temp0)
        )(else
            -1
        )
)

// A common base class for GUI controls.
(class Control of Object
    (properties
        type 0
        state $0000
        nsTop 0
        nsLeft 0
        nsBottom 0
        nsRight 0
        key 0			// The keyboard key associated with this control.
        said 0
        value 0			// Arbitrary value associated with this control.
    )

    (method (doit)
        return value
    )


    (method (enable fEnable)
        (if (fEnable)
            = state (| state csENABLED)
        )(else
            = state (& state (bnot csENABLED))
        )
    )


    (method (select fSelect)
        (if (fSelect)
            = state (| state csSELECTED)
        )(else
            = state (& state (bnot csSELECTED))
        )
        (self:draw())
    )


    (method (handleEvent pEvent)
        (var temp0, pEventType, temp2)
        (if ((send pEvent:claimed))
            return 0
        )
        = temp0 0
        (if ((& state csENABLED) and (((== (= pEventType (send pEvent:type)) 4) and (== (send pEvent:message) key)) or ((== pEventType evMOUSEBUTTON) and (self:check(pEvent)))))
            (send pEvent:claimed(TRUE))
            = temp0 (self:track(pEvent))
        )
        return temp0
    )

	// Returns TRUE if the x and y of the event lie inside the control.
    (method (check pEvent)
        return 
            (if (((>= (send pEvent:x) nsLeft) and (>= (send pEvent:y) nsTop)) and (< (send pEvent:x) nsRight))
                < (send pEvent:y) nsBottom
            )(else
                0
            )
    )

	// Highlights the control if the mouse is over it.
    (method (track pEvent)
        (var temp0, temp1)
        return 
            (if (== evMOUSEBUTTON (send pEvent:type))
                = temp1 0
                (do 
                    = pEvent (Event:new(evPEEK))
                    (send pEvent:localize())
                    = temp0 (self:check(pEvent))
                    (if (<> temp0 temp1)
                        HiliteControl(self)
                        = temp1 temp0
                    )
                    (send pEvent:dispose())
                ) while (not not MouseStillDown())
                (if (temp0)
                    HiliteControl(self)
                )
                return temp0
            )(else
                return self
            )
    )


    (method (setSize)
    )


    (method (move param1 param2)
        = nsRight (+ nsRight param1)
        = nsLeft (+ nsLeft param1)
        = nsTop (+ nsTop param2)
        = nsBottom (+ nsBottom param2)
    )


    (method (moveTo param1 param2)
        (self:move((- param1 nsLeft) (- param2 nsTop)))
    )

	// Draws the control.
    (method (draw)
        DrawControl(self)
    )

	/*
		:param number theType: ctlBUTTON, ctlTEXT, ctlEDIT, ctlICON or ctlSELETOR.
		:returns: TRUE if this control is of the type specified.
	*/
    (method (isType theType)
        return == type theType
    )

	/*
		:param number stateFlags: Any combination of csENABLED, csEXIT, csFILTER or csSELECTED.
		:returns: A non-zero value if the control has this state.
	*/
    (method (checkState stateFlags)
        return & state stateFlags
    )


    (method (cycle)
    )
)

// A control that simply displays text.
(class DText of Control
    (properties
        type ctlTEXT
        state $0000
        nsTop 0
        nsLeft 0
        nsBottom 0
        nsRight 0
        key 0			// The keyboard key associated with this control.
        said 0
        value 0			// Arbitrary value associated with this control.
        text 0			// The text.
        font 1			// The font.
        mode alLEFT		// alLEFT, alCENTER or alRIGHT.
        rects 0
    )

    (method (new)
        (var temp0)
        (send ((super:new())):
            font(gFont)
            yourself()
        )
    )


    (method (dispose param1)
        (if (text and (not paramTotal or not param1))
            Memory(memFREE (self:text))
        )
        (if (rects)
            Memory(memFREE (self:rects))
        )
        (super:dispose())
    )


    (method (handleEvent pEvent)
        (var temp0, temp1, temp2, temp3, temp4)
        (asm
            lag     gTextCode
            bnt     code_0455
            pToa    rects
            bnt     code_0455
            pushi   3
            pushi   #type
            pushi   0
            lap     pEvent
            send    4
            push    
            pushi   1
            pushi   256
            calle   IsOneOf, 6
            bt      code_03b6
            pushi   #type
            pushi   0
            lap     pEvent
            send    4
            push    
            ldi     4
            eq?     
            bnt     code_0455
            pushi   #message
            pushi   0
            lap     pEvent
            send    4
            push    
            ldi     13
            eq?     
            bnt     code_0455
code_03b6:  ldi     65535
            sat     temp0
            pushi   #globalize
            pushi   0
            pushi   73
            pushi   1
            pushi   1
            lap     pEvent
            send    10
code_03c6:  pushi   2
            pTos    rects
            lst     temp0
            ldi     1
            add     
            push    
            calle   WordAt, 4
            push    
            ldi     $7777
            ne?     
            bnt     code_0455
            pushi   2
            pTos    rects
            +at     temp0
            push    
            calle   WordAt, 4
            sat     temp2
            pushi   2
            pTos    rects
            +at     temp0
            push    
            calle   WordAt, 4
            sat     temp1
            pushi   2
            pTos    rects
            +at     temp0
            push    
            calle   WordAt, 4
            sat     temp4
            pushi   2
            pTos    rects
            +at     temp0
            push    
            calle   WordAt, 4
            sat     temp3
            lst     temp2
            pushi   #x
            pushi   0
            lap     pEvent
            send    4
            le?     
            bnt     code_03c6
            pprev   
            lat     temp4
            le?     
            bnt     code_03c6
            lst     temp1
            pushi   #y
            pushi   0
            lap     pEvent
            send    4
            le?     
            bnt     code_03c6
            pprev   
            lat     temp3
            le?     
            bnt     code_03c6
            pushi   57
            pushi   #x
            lst     temp0
            ldi     4
            div     
            push    
            lag     gTextCode
            send    6
            pushi   #type
            pushi   1
            pushi   0
            pushi   73
            pushi   1
            pushi   0
            lap     pEvent
            send    12
            jmp     code_0455
            jmp     code_03c6
code_0455:  pushi   #handleEvent
            pushi   1
            lsp     pEvent
            super   Control, 6
            ret     
        )
    )


    (method (setSize param1)
        (var temp0[4])
        TextSize(@temp0[0] text font 
            (if (paramTotal)
                param1
            )(else
                0
            )
)
        = nsBottom (+ nsTop temp0[2])
        = nsRight (+ nsLeft temp0[3])
    )


    (method (draw)
        = rects DrawControl(self)
    )
)

(class Dialog of List
    (properties
        elements 0
        size 0
        text 0
        font 0
        window 0
        theItem 0
        nsTop 0
        nsLeft 0
        nsBottom 0
        nsRight 0
        time 0
        caller 0
        seconds 0
        lastSeconds 0
        eatTheMice 0
        lastTicks 0
    )

    (method (doit param1)
        (var temp0, temp1, temp2)
        = gGameTime (+ gTickOffset GetTime())
        = temp2 0
        (self:eachElementDo(#init))
        (if (theItem)
            (send theItem:select(FALSE))
        )
        = theItem 
            (if (paramTotal and param1)
                param1
            )(else
                (self:firstTrue(#checkState csENABLED))
            )
        (if (theItem)
            (send theItem:select(TRUE))
        )
        (if (not theItem)
            = eatTheMice gEatTheMice
            = lastTicks GetTime()
        )(else
            = eatTheMice 0
        )
        = temp1 0
        
       
        (while (not temp1)
            = gGameTime (+ gTickOffset GetTime())
            (self:eachElementDo(#cycle))
            = temp0 (send ((Event:new())):localize())
            (if (eatTheMice)
                --eatTheMice
               
                (if (== (send temp0:type) 1)
                    (send temp0:type(0))
                )
                (while (== lastTicks GetTime())
                )
                = lastTicks GetTime()
            )
            (self:eachElementDo(#perform checkHiliteCode self temp0))
            = temp1 (self:handleEvent(temp0))
            (send temp0:dispose())
            (if ((self:check()))
                break
            )
            (if (== temp1 -2)
                break
            )
            Wait(1)
        )
        return temp1
    )


    (method (dispose)
        (var theCaller)
        (self:
            eachElementDo(#dispose)
            release()
        )
        (if (== self gDialog)
            SetPort(gOldPort)
            = gDialog 0
            = gOldPort NULL
        )
        (if (window)
            (send window:dispose())
            = window 0
        )
        = theItem 0
        = theCaller caller
        (super:dispose())
        (if (theCaller)
            (send theCaller:cue())
        )
    )


    (method (open param1 param2)
        (if (PicNotValid() and gCast)
            Animate((send gCast:elements) 0)
        )
        = window (send window:new())
        (send window:
            top(nsTop)
            left(nsLeft)
            bottom(nsBottom)
            right(nsRight)
            title(text)
            type(param1)
            priority(param2)
            open()
        )
        = seconds time
        (self:draw())
    )


    (method (draw)
        (self:eachElementDo(#draw))
    )


    (method (advance)
        (var temp0, dialogFirst)
        (if (theItem)
            (send theItem:select(FALSE))
            = dialogFirst (self:contains(theItem))
            (while (TRUE)
                = dialogFirst (self:next(dialogFirst))
                (if (not dialogFirst)
                    = dialogFirst (self:first())
                )
                = theItem NodeValue(dialogFirst)
                (if (& (send theItem:state) csENABLED)
                    break
                )
            )
            (send theItem:select(TRUE))
            (send gGame:setCursor(gCursorNumber 1 (+ (send theItem:nsLeft) (/ (- (send theItem:nsRight) (send theItem:nsLeft)) 2)) (- (send theItem:nsBottom) 3)))
        )
    )


    (method (retreat)
        (var temp0, dialogLast)
        (if (theItem)
            (send theItem:select(FALSE))
            = dialogLast (self:contains(theItem))
            (while (TRUE)
                = dialogLast (self:prev(dialogLast))
                (if (not dialogLast)
                    = dialogLast (self:last())
                )
                = theItem NodeValue(dialogLast)
                (if (& (send theItem:state) csENABLED)
                    break
                )
            )
            (send theItem:select(TRUE))
            (send gGame:setCursor(gCursorNumber 1 (+ (send theItem:nsLeft) (/ (- (send theItem:nsRight) (send theItem:nsLeft)) 2)) (- (send theItem:nsBottom) 3)))
        )
    )


    (method (move param1 param2)
        = nsRight (+ nsRight param1)
        = nsLeft (+ nsLeft param1)
        = nsTop (+ nsTop param2)
        = nsBottom (+ nsBottom param2)
    )


    (method (moveTo param1 param2)
        (self:move((- param1 nsLeft) (- param2 nsTop)))
    )


    (method (center)
        (self:moveTo((+ (send window:brLeft) (/ (- (- (send window:brRight) (send window:brLeft)) (- nsRight nsLeft)) 2)) (+ (send window:brTop) (/ (- (- (send window:brBottom) (send window:brTop)) (- nsBottom nsTop)) 2))))
    )


    (method (setSize)
        (var dialogFirst, temp1, theNsTop[4])
        (if (text)
            TextSize(@theNsTop[0] text font -1 0)
            = nsTop theNsTop[0]
            = nsLeft theNsTop[1]
            = nsBottom theNsTop[2]
            = nsRight theNsTop[3]
        )(else
            = nsRight (= nsBottom (= nsLeft (= nsTop 0)))
        )
        = dialogFirst (self:first())
        (while (dialogFirst)
            = temp1 NodeValue(dialogFirst)
            (if (< (send temp1:nsLeft) nsLeft)
                = nsLeft (send temp1:nsLeft)
            )
            (if (< (send temp1:nsTop) nsTop)
                = nsTop (send temp1:nsTop)
            )
            (if (> (send temp1:nsRight) nsRight)
                = nsRight (send temp1:nsRight)
            )
            (if (> (send temp1:nsBottom) nsBottom)
                = nsBottom (send temp1:nsBottom)
            )
            = dialogFirst (self:next(dialogFirst))
        )
        = nsRight (+ nsRight 4)
        = nsBottom (+ nsBottom 4)
        (self:moveTo(0 0))
    )


    (method (handleEvent pEvent)
        (var theTheItem, pEventType, pEventMessage)
        
        (if (& (send pEvent:type) evJOYSTICK)
            (switch ((send pEvent:message))
                (case JOY_DOWN
                    (send pEvent:
                        type(evKEYBOARD)
                        message(KEY_DOWN)
                    )
                )
                (case JOY_UP
                    (send pEvent:
                        type(evKEYBOARD)
                        message(KEY_UP)
                    )
                )
                (case JOY_LEFT
                    (send pEvent:
                        type(evKEYBOARD)
                        message(KEY_LEFT)
                    )
                )
                (case JOY_RIGHT
                    (send pEvent:
                        type(evKEYBOARD)
                        message(KEY_RIGHT)
                    )
                )
            )
        )
        = pEventType (send pEvent:type)
        = pEventMessage (send pEvent:message)
        = theTheItem (self:firstTrue(#handleEvent pEvent))
        (if (theTheItem)
            EditControl(theItem 0)
            (if (not (send theTheItem:checkState(csEXIT)))
                (if (theItem)
                    (send theItem:select(FALSE))
                )
                (send (= theItem theTheItem):select(TRUE))
                (send theTheItem:doit())
                = theTheItem 0
            )(else
                return theTheItem
            )
        )(else
            = pEventType (send pEvent:type)
            = pEventMessage (send pEvent:message)
            = theTheItem 0
            (if ((((== pEventType 256) or ((== pEventType evKEYBOARD) and (== pEventMessage KEY_RETURN))) and theItem) and (send theItem:checkState(csENABLED)))
                = theTheItem theItem
                EditControl(theItem 0)
                (send pEvent:claimed(TRUE))
            )(else
                (if ((== pEventType evKEYBOARD) and (== pEventMessage KEY_ESCAPE))
                    (send pEvent:claimed(TRUE))
                    = theTheItem -1
                )(else
                    (if (not (self:firstTrue(#checkState csENABLED)) and (((== pEventType evKEYBOARD) and (== pEventMessage KEY_RETURN)) or IsOneOf(pEventType evMOUSEBUTTON 256)))
                        (send pEvent:claimed(TRUE))
                        = theTheItem -2
                    )(else
                        (if (((IsObject(theItem) and (send theItem:isType(ctlEDIT))) and (== pEventType evKEYBOARD)) and (== pEventMessage KEY_RIGHT))
                            (if (>= (send theItem:cursor) StrLen((send theItem:text)))
                                (self:advance())
                            )(else
                                EditControl(theItem pEvent)
                            )
                        )(else
                            (if (((IsObject(theItem) and (send theItem:isType(ctlEDIT))) and (== pEventType evKEYBOARD)) and (== pEventMessage KEY_NUMPAD4))
                                (if (<= (send theItem:cursor) 0)
                                    (self:retreat())
                                )(else
                                    EditControl(theItem pEvent)
                                )
                            )(else
                                (if ((== pEventType evKEYBOARD) and IsOneOf(pEventMessage 9 19712 20480))
                                    (send pEvent:claimed(TRUE))
                                    (self:advance())
                                )(else
                                    (if ((== pEventType evKEYBOARD) and IsOneOf(pEventMessage 3840 19200 18432))
                                        (send pEvent:claimed(TRUE))
                                        (self:retreat())
                                    )(else
                                        EditControl(theItem pEvent)
                                    )
                                )
                            )
                        )
                    )
                )
            )
        )
        return theTheItem
    )


    (method (check)
        (var theLastSeconds)
        return 
            (if (seconds and (<> lastSeconds (= theLastSeconds GetTime(gtTIME_OF_DAY))))
                = lastSeconds theLastSeconds
                return not --seconds
            )(else
                0
            )
    )

)
(instance checkHiliteCode of Code
    (properties)

    (method (doit theControl theDialog pEvent)
        (if (((& (send theControl:state) csENABLED) and (send theControl:check(pEvent))) and not (& (send theControl:state) csSELECTED))
            (send ((send theDialog:theItem)):select(FALSE))
            (send theDialog:theItem(theControl))
            (send theControl:select(TRUE))
        )
    )

)
