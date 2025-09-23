/*
	This is the main game script. It contains the main game class, all the global variables, and
	a number of useful procedures.
	
	In addition to the above, it contains the icon instances for the icons
	in the control bar. It also contains the crucial default Messager
	and its findTalker method (used for mapping talker numbers to a Talker or Narrator instance).
*/
(version 2)
(include "sci.sh")
(include "game.sh")
(include "0.shm")
(exports
    0 SQ5
    1 Btest
    2 Bset
    3 Bclear
    4 RestorePreviousHandsOn
    5 IsObjectOnControl
    6 SetUpEgo
    7 AddToScore
    8 AimToward
    9 Die
    10 ScoreFlag
    11 HideStatus
    12 DebugPrint
    13 AddPolygonsToRoom
    14 CreateNewPolygon
)
(use "ColorInit")
(use "Smopper")
(use "GameEgo")
(use "ScrollableInventory")
(use "ScrollInsetWindow")
(use "DColorButton")
(use "SpeakWindow")
(use "Print")
(use "DialogControls")
(use "Messager")
(use "Talker")
(use "PseudoMouse")
(use "Scaler")
(use "BorderWindow")
(use "IconItem")
(use "RandCycle")
(use "PolyPath")
(use "Polygon")
(use "StopWalk")
(use "Timer")
(use "Grooper")
(use "Sound")
(use "Game")
(use "User")
(use "System")
(use "File")
(script 0)

(define STARTING_ROOM 100)

(local
    gEgo				// The object for the player's ego.
    gGame				// The game object.
    gRoom				// The current room object.
    global3		// Unused
    gQuitGame = FALSE
    gCast
    gRegions			// The current regions.
    gTimers				// The current timers.
    gSounds				// The current sounds.
    gInv				// The inventory.
    gAddToPics
    gRoomNumber			// The current room number
    gPreviousRoomNumber
    gNewRoomNumber
    gDebugOnNextRoom
    gScore				// The player's current score.
    gMaxScore			// The maximum score.
    gTextCode
    gCuees
    gCursorNumber
    gNormalCursor =     999
    gWaitCursor =     997
    gFont =     1		// Main font number.
    gSmallFont =     4	// Small font number.
    gPEvent				// The current event.
    gDialog				// The current Print dialog.
    gBigFont =     1	// Big font number.
    gVersion
    gSaveDir
    gPicAngle
    gFeatures
    gUseSortedFeatures
    gPicNumber =     -1
    gDoMotionCue
    gWindow
    global39	// Unused
    global40	// Unused
    gOldPort
    gDebugFilename[21] 		// debug filename
    gGameControls			// The main GameControls class.
    gFeatureInit			// Code that initializes all features.
    gDoVerbCode
    gApproachCode
    gEgoUseObstacles = 1	// Default Motion type for ego (0: MoveTo, 1: PolyPath, ...)
    gIconBar
    gPEventX				// Current event's x value.
    gPEventY				// Current event's y value.
    gOldKH
    gOldMH
    gOldDH
    gPseudoMouse
    gTheDoits
    gEatTheMice =     60	// Number of ticks before we mouse
    gUser
    gSyncBias				// Something to do with lip-sync.
    gTheSync
    global83				// Something to do with audio narration.
    gFastCast
    gInputFont
    gTickOffset 			// Something to do with time (ticks per frame?)
    gGameTime
    gNarrator				// Default Narrator.
    gMessageType =    $0001	// Talker flags: 0x1 (text) and 0x2 (audio).
    gMessager
    gPrints
    gWalkHandler
    gTextReadSpeed =     2
    gAltPolyList
    gColorDepth
    gPolyphony
    gStopGroop
    global107
    gCurrentIcon
    gGUserCanControl
    gGUserCanInput
    gCheckedIcons
    gState
    gNewSpeakWindow
    gWindow2
    gDeathReason
    gMusic1
    gDongle = 1234				// This variable CAN'T MOVE
    gMusic2
    gCurrentTalkerNumber
    gGEgoMoveSpeed
    gColorWindowForeground
    gColorWindowBackground
    gLowlightColor
    gDefaultEgoView =     0		// The default view resource for the ego
    gRegister
    gFlags[14]					// Start of bit set. Room for 14 x 16 = 224 flags.
    gEdgeDistance = 10			// Margin around screen to make it easier to walk the ego to the edge.
    gDebugOut
)

/*
	Tests a boolean game flag.
	
	:param number flag: The number of the flag to test.
	:returns: TRUE if the flag is set, otherwise FALSE.
	
	Example usage::
	
		(if (not (Btest FLAG_OpenedSewer))
			(Prints {You can't enter, the sewer is closed.})
		)
*/
(procedure public (Btest flag)
    return & gFlags[(/ flag 16)] (>> $8000 (% flag 16))
)

/*
	Sets a boolean game flag.
	
	:param number flag: The number of the flag to set.
	:returns: The previous value of the flag (TRUE or FALSE).
	
	Example usage::
	
		(V_DO
			(Bset FLAG_OpenedSewer)
			(sewer setCel: 3)
		)
*/
(procedure public (Bset flag)
    (var temp0)
    = temp0 Btest(flag)
    = gFlags[(/ flag 16)] (| gFlags[(/ flag 16)] (>> $8000 (% flag 16)))
    return temp0
)

/*
	Clears a boolean game flag.
	
	:param number flag: The number of the flag to clear.
	:returns: The previous value of the flag (TRUE or FALSE).
*/
(procedure public (Bclear flag)
    (var temp0)
    = temp0 Btest(flag)
    = gFlags[(/ flag 16)] (& gFlags[(/ flag 16)] bnot (>> $8000 (% flag 16)))
    return temp0
)

(procedure public (RestorePreviousHandsOn)
    (var temp0)
    (send gUser:
        canControl(gGUserCanControl)
        canInput(gGUserCanInput)
    )
    = temp0 0
    (while (< temp0 8)
        (if (& gCheckedIcons (>> $8000 temp0))
            (send gIconBar:disable(temp0))
        )
        ++temp0
    )
)

/*
	Tests if the origin of an object is on the control color.
	
	:param heapPtr theActor: An :class:`Actor` object.
	:param number ctlColor: A control color (such as ctlLIME or ctlWHITE).
	:returns: TRUE if the object is on the control color, FALSE otherwise.
*/
(procedure public (IsObjectOnControl theActor ctlColor)
    return 
        (if (& (send theActor:onControl(TRUE)) ctlColor)
            return 1
        )(else
            0
        )
)

/*
.. function:: SetUpEgo([theView theLoop])

	Used to set up the ego, generally in a room's init() method.
	
	:param number theView: The view to use, or -1 to use gDefaultEgoView.
	:param number theLoop: The loop to use.
*/
(procedure public (SetUpEgo theView theLoop)
    (if ((> paramTotal 0) and (<> theView -1))
        (send gEgo:view(theView))
        (if ((> paramTotal 1) and (<> theLoop -1))
            (send gEgo:loop(theLoop))
        )
    )(else
        (send gEgo:view(gDefaultEgoView))
        (if ((> paramTotal 1) and (<> theLoop -1))
            (send gEgo:loop(theLoop))
        )
    )
    (if ((send gEgo:looper))
        (send ((send gEgo:looper)):dispose())
    )
    (send gEgo:
        setStep(5 2)
        illegalBits(0)
        ignoreActors(0)
        setSpeed(gGEgoMoveSpeed)
        //signal(| (send gEgo:signal) $1000)
        heading(
            (switch ((send gEgo:loop))
                (case 0
                    90
                )
                (case 1
                    270
                )
                (case 2
                    180
                )
                (case 3
                    0
                )
                (case 4
                    135
                )
                (case 5
                    225
                )
                (case 6
                    45
                )
                (case 7
                    315
                )
			)
			   )
    )
    (send gEgo:
        setLoop(-1)
        setLoop(stopGroop)
        setPri(-1)
        setMotion(NULL)
        state(| (send gEgo:state) $0002)
    )
)

/*
.. function:: AimToward(theObj otherObj [cueObj])

.. function:: AimToward(theObj otherObj faceEachOther [cueObj])

.. function:: AimToward(theObj x y [cueObj])

	Changes the heading of theObj so it faces another object or position.
	Optionally causes the other object to face the first object.

	:param heapPtr theObj: The object that is being aimed.
	:param heapPtr otherObj: The target object.
	:param boolean faceEachOther: If TRUE, the otherObj will also be made to face theObj.
	:param number x: The target x.
	:param number y: The target x.
	:param heapPtr cueObj: Optional object to be cue()'d.
*/
(procedure public (AimToward theObj param2 param3 param4)
    (var theAngle, theX, theY, cueObject, someFlag)
    = cueObject 0
    = someFlag 0
    (if (IsObject(param2))
        = theX (send param2:x)
        = theY (send param2:y)
        (if (> paramTotal 2)
            (if (IsObject(param3))
                = cueObject param3
            )(else
                = someFlag param3
            )
            (if (== paramTotal 4)
                = cueObject param4
            )
        )
    )(else
        = theX param2
        = theY param3
        (if (== paramTotal 4)
            = cueObject param4
        )
    )
    (if (someFlag)
        AimToward(param2 theObj)
    )
    = theAngle GetAngle((send theObj:x) (send theObj:y) theX theY)
    (send theObj:setHeading(theAngle 
        (if (IsObject(cueObject))
            cueObject
        )(else
            0
        )
						   ))
)

/*
.. function:: Die([theDeathReason])

	Causes the ego to die. The global variable gDeathReason will be set to the death reason.
	
	:param number theDeathReason: An arbitrary numerical value to be interpreted by the DeathRoom.sc script.
*/
(procedure public (Die theDeathReason)
    (if (not paramTotal)
        = gDeathReason 1
    )(else
        = gDeathReason theDeathReason
    )
    (send gRoom:newRoom(DEATH_SCRIPT))
)

/*
	Adds an amount to the player's current score.
	
	:param number amount: The amount to add to the score (can be negative).
*/
(procedure public (AddToScore amount)
    = gScore (+ gScore amount)
    (statusLineCode:doit())
    (rm0Sound:
        priority(15)
        number(1000)
        loop(1)
        flags(1)
        play()
    )
)

/*
	Adds an amount to the player's current score. A flag (one used with
	:func:`BSet`, :func:`BClear` or :func:`BTest`) must be provided. This
	ensures that a score is only added once.
	
	:param number flag: A flag indicating what this score is for.
	:param number amount: The amount to add to the score.
*/
(procedure public (ScoreFlag flag amount)
    (if (not Btest(flag))
    	AddToScore(amount)
        Bset(flag)
    )
)

/*
	Hides the status bar.
*/
(procedure public (HideStatus)
    (var temp0)
    = temp0 GetPort()
    SetPort(-1)
    Graph(grFILL_BOX 0 0 10 320 VISUAL 0 -1 -1)
    Graph(grUPDATE_BOX 0 0 10 320 VISUAL)
    SetPort(temp0)
)

/*
.. function:: DebugPrint(theText [params ...])

	Prints a debug message that can be displayed in SCI Companion. The text may contain the following formatting
	characters:
	
	%d
		Formats a number in decimal.
		
	%x
		Formats a number in hexadecimal.

	%s
		Formats a string.

	:param string theText: A string of text containing formatting characters.
	
	Example usage::
	
		(DebugPrint {You are in room %d} gNewRoomNumber)
*/
(procedure public (DebugPrint params)
	(if (gDebugOut)
		(send gDebugOut:debugPrint(rest params))
	)
)

(procedure (CreateNewPolygonHelper polyBuffer nextPoly)
	(var newPoly, pointCount)
	(= newPoly (Polygon:new()))
	(= pointCount (Memory(memPEEK (+ polyBuffer 2))))
	(send newPoly:
		dynamic(FALSE)
		type(Memory(memPEEK polyBuffer))
		size(pointCount)
		// Use the points directly from the buffer:
		points(+ polyBuffer 4)
	)
	
	// Tell the caller the position of the next poly, if they care:
	(if (> paramTotal 1)
		Memory(memPOKE nextPoly (+ polyBuffer (+ 4 (* 4 pointCount))))
	)
	return newPoly
)

/*
	Creates :class:`Polygon` objects based on the point lists in polyBuffer and adds
	them to the room's obstacles.
	
	:param heapPtr polyBuffer: An array with polygon points.
	
	Example usage::
	
		(AddPolygonsToRoom @P_ThePolygons)
	
	The array begins with a number indicating how many polygons there are. This is followed
	by the following information for each polygon:
	
		- A number expressing the type of the polygon (e.g. PBarredAccess).
		- A number indicating how many points are in the polygon.
		- (x y) pairs of numbers for each point.
		
	Example::
	
		[P_ThePolygons 19] = [2 PContainedAccess 4 319 189 319 50 0 50 0 189 PBarredAccess 3 319 189 319 50 0 50]
		
	See also: :doc:`/polygons`.		
*/
(procedure public (AddPolygonsToRoom polyBuffer)
	(var polyCount)
	(if (<u polyBuffer 100)
		Prints("polyBuffer is not a pointer. Polygon ignored.")
	)(else
		(= polyCount Memory(memPEEK polyBuffer))
		(+= polyBuffer 2)
		(while (polyCount)
			(send gRoom:addObstacle(
					(if (== polyCount 1)
						CreateNewPolygonHelper(polyBuffer)
					)
					(else
						CreateNewPolygonHelper(polyBuffer @polyBuffer)
					)
					))
			--polyCount
		)
	 )
)

/*
.. function:: CreateNewPolygon(polyBuffer [nextPolyOptional])

	Creates a new polygon object.
	
	:param heapPtr polyBuffer: An array with polygon points.
	:param heapPtr nextPolyOptional: An optional pointer that receives the position of the next polygon in the buffer.
	
	Example usage::
	
		(aRock setOnMeCheck: omcPOLYGON (CreateNewPolygon @P_Rock))

	The array consists of the following:	
	
		- A number expressing the type of the polygon (e.g. PBarredAccess).
		- A number indicating how many points are in the polygon.
		- (x y) pairs of numbers for each point.
		
	Example::
	
		[P_Rock 10] = [PContainedAccess 4 319 189 319 50 0 50 0 189]
		
	See also: :doc:`/polygons`.
*/
(procedure public (CreateNewPolygon polyBuffer nextPolyOptional)
	(var polyCount)
	(if (<u polyBuffer 100)
		Prints("polyBuffer is not a pointer. Polygon ignored.")
		return NULL
	)(else
		(= polyCount Memory(memPEEK polyBuffer))
		(+= polyBuffer 2)
		return CreateNewPolygonHelper(polyBuffer rest nextPolyOptional)
	 )
)

(instance rm0Sound of Sound
    (properties
        priority 15
    )
)
(instance music1 of Sound
    (properties
        flags $0001
    )
)
(instance music2 of Sound
    (properties
        flags $0001
    )
)
(instance stopGroop of GradualLooper
    (properties)
)
(instance egoStopWalk of FiddleStopWalk
    (properties)
)
(instance ego of GameEgo
    (properties)
)
(instance statusLineCode of Code
    (properties)

    (method (doit)
        (var temp0[50], temp50[50], temp100)
        = temp100 GetPort()
        SetPort(-1)
        Graph(grFILL_BOX 0 0 10 320 VISUAL 5 -1 -1)
        Graph(grUPDATE_BOX 0 0 10 320 VISUAL)
        Message(msgGET 0 N_TITLEBAR 0 0 1 @temp0)
        Format(@temp50 "%s %d" @temp0 gScore)
        Display(@temp50 dsCOORD 4 0 dsFONT gFont dsCOLOR 6)
        Display(@temp50 dsCOORD 6 2 dsFONT gFont dsCOLOR 4)
        Display(@temp50 dsCOORD 5 1 dsFONT gFont dsCOLOR 0)
        Graph(grDRAW_LINE 0 0 0 319 7 -1 -1)
        Graph(grDRAW_LINE 0 0 9 0 6 -1 -1)
        Graph(grDRAW_LINE 9 0 9 319 4 -1 -1)
        Graph(grDRAW_LINE 0 319 9 319 3 -1 -1)
        Graph(grUPDATE_BOX 0 0 10 319 VISUAL)
        SetPort(temp100)
    )

)
(instance templateIconBar of IconBar
    (properties)

    (method (show)
        (if (IsObject(curInvIcon))
            (send curInvIcon:loop(2))
        )
        (super:show())
        (if (IsObject(curInvIcon))
            (send curInvIcon:loop(1))
        )
    )

    (method (hide param1)
        (super:hide(rest param1))
        (send gGame:setCursor(gCursorNumber 1))
    )

    (method (noClickHelp)
        (var temp0, temp1, temp2, temp3, winEraseOnly)
        = temp2 0
        = temp1 temp2
        = temp3 GetPort()
        = winEraseOnly (send gWindow:eraseOnly)
        (send gWindow:eraseOnly(1))
        (while (not (send (= temp0 (send ((send gUser:curEvent)):new())):type))
            (if (not (self:isMemberOf(IconBar)))
                (send temp0:localize())
            )
            = temp2 (self:firstTrue(#onMe temp0))
            (if (temp2)
                (if ((<> (= temp2 (self:firstTrue(#onMe temp0))) temp1) and (send temp2:helpVerb))
                    = temp1 temp2
                    (if (gDialog)
                        (send gDialog:dispose())
                    )
                    (Print:
                        font(gFont)
                        width(250)
                        addText((send temp2:noun) (send temp2:helpVerb) 0 1 0 0 (send temp2:modNum))
                        modeless(1)
                        init()
                    )
                    Animate((send gCast:elements) 0)
                    SetPort(temp3)
                )
            )(else
                (if (gDialog)
                    (send gDialog:dispose())
                    Animate((send gCast:elements) 0)
                )(else
                    = temp1 0
                )
            )
            (send temp0:dispose())
        )
        (send gWindow:eraseOnly(winEraseOnly))
        (send gGame:setCursor(999 1))
        (if (gDialog)
            (send gDialog:dispose())
            Animate((send gCast:elements) 0)
        )
        SetPort(temp3)
        (if (not (send helpIconItem:onMe(temp0)))
            (self:dispatchEvent(temp0))
        )
    )
)

// In order for this game to run in ScummVM, the game name needs to
// be a known one (e.g. SQ5)

/*
	The main game class. This subclasses :class:`Game` and adds game-specific functionality.
*/
(class public SQ5 of Game
    (properties
        script 0
        printLang 1
        _detailLevel 3
        panelObj 0
        panelSelector 0
        handsOffCode 0
        handsOnCode 0
    )

	/*
		Modify this to set up any initial state the game needs. Among the things set here are:
		
		- The maximum score.
		- Text colors and fonts used in messages.
		- The action icons.
		- The default game cursor.
		
	*/
    (method (init)
        (var temp0[7], temp7)
        
        (send (ScriptID(INVENTORY_SCRIPT 0)):init())
        (super:init())
        = gEgo ego
        (User:
            alterEgo(gEgo)
            canControl(0)
            canInput(0)
        )
        = gMessageType $0001
        = gUseSortedFeatures TRUE
        = gPolyphony DoSound(sndGET_POLYPHONY)
        = gMaxScore 5000
        = gFont 1605
        = gGEgoMoveSpeed 6
        = gEatTheMice 30
        = gTextReadSpeed 2
        = gColorDepth Graph(grGET_COLOURS)
        = gStopGroop stopGroop
        = gPseudoMouse PseudoMouse
        (send gEgo:setLoop(gStopGroop))
        // The position of these font resource numbers correspond to font codes used in messages:
        TextFonts(1605 1605 1605 1605 1605 0)
        // These correspond to color codes used in messages (values into global palette):
        TextColors(0 15 26 31 34 52 63)
        = gVersion "x.yyy.zzz"
        = temp7 FileIO(fiOPEN "version" fOPENFAIL)
        FileIO(fiREAD_STRING gVersion 11 temp7)
        FileIO(fiCLOSE temp7)
        ColorInit()
        DisposeScript(COLORINIT_SCRIPT)
        = gNarrator templateNarrator
        = gWindow mainWindow
        = gWindow2 mainWindow
        = gMessager testMessager
        = gNewSpeakWindow (SpeakWindow:new())
        (send gWindow:
            color(gColorWindowForeground)
            back(gColorWindowBackground)
        )
        (send gGame:
            setCursor(gCursorNumber TRUE 304 172)
            detailLevel(3)
        )
        = gMusic1 music1
        (send gMusic1:
            //number(1)
            owner(self)
            flags(1)
            init()
        )
        = gMusic2 music2
        (send gMusic2:
            //number(1)
            owner(self)
            flags(1)
            init()
        )
        = gIconBar templateIconBar
        (send gIconBar:
        	// These correspond to ICONINDEX_*** in game.sh
            add(icon0 icon1 icon2 icon3 icon4 icon6 icon7 icon8 icon9)
            eachElementDo(#init)
            eachElementDo(#highlightColor 0)
            eachElementDo(#lowlightColor 5)
            curIcon(icon0)
            useIconItem(icon6)
            helpIconItem(icon9)
            walkIconItem(icon0)
            disable(ICONINDEX_CURITEM)
            state(3072)
            disable()
        )
        = gNormalCursor 999
        = gWaitCursor 996
        = gDoVerbCode lb2DoVerbCode
        = gFeatureInit lb2FtrInit
        = gApproachCode lb2ApproachCode
    )

    (method (doit param1)
        (if (GameIsRestarting())
            (if (IsOneOf(gRoomNumber TITLEROOM_SCRIPT))
                HideStatus()
            )(else
                (statusLineCode:doit())
            )
            = gColorDepth Graph(grGET_COLOURS)
        )
        (super:doit(rest param1))
    )


    (method (play)
    	(var deleteMe, debugRoom, theStartRoom)
        = gGame self
        = gSaveDir GetSaveDir()
        
        (if (not GameIsRestarting())
            GetCWD(gSaveDir)
        )
        (self:
            setCursor(gWaitCursor 1)
            init()
        )

		(= theStartRoom STARTING_ROOM)
		(if (not GameIsRestarting())
	        (if (FileIO(fiEXISTS "sdebug.txt"))
	        	= gDebugOut ScriptID(DEBUGOUT_SCRIPT 0)
	        	DebugPrint("Debugger enabled")
	        	= debugRoom (send gDebugOut:init("sdebug.txt"))
	        	(if (<> debugRoom -1)
	        		(= theStartRoom debugRoom)
	        		(send gGame:handsOn())
	        		DebugPrint("Starting in room %d" theStartRoom)
				)
			)
		)
		
        (self:newRoom(theStartRoom))
        
        (while (not gQuitGame)
            (self:doit())
        )
    )


    (method (startRoom param1)
        (var temp0[4])
        (if (IsOneOf(param1 TITLEROOM_SCRIPT))
            HideStatus()
        )(else
            (statusLineCode:doit())
        )
        (if (gPseudoMouse)
            (send gPseudoMouse:stop())
        )
        (send (ScriptID(DISPOSECODE_SCRIPT)):doit(param1))
        
        (super:startRoom(param1))
    )


    (method (restart param1)
        (var temp0, temp1)
        = temp1 (send ((send gIconBar:curIcon)):cursor)
        (send gGame:setCursor(999))
        = temp0 (Print:
                font(gFont)
                width(75)
                window(gWindow)
                mode(1)
                addText(N_RESTART V_LOOK 0 1 0 0 0)
                addColorButton(1 N_RESTART V_LOOK 0 2 0 40 0)
                addColorButton(0 N_RESTART V_LOOK 0 3 0 50 0)
                init()
            )
        (if (temp0)
            (super:restart(rest param1))
        )(else
            (send gGame:setCursor(temp1))
        )
    )


    (method (restore param1)
        (var temp0[2])
        (super:restore(rest param1))
        (send gGame:setCursor((send ((send gIconBar:curIcon)):cursor)))
    )


    (method (save param1)
        (super:save(rest param1))
        (send gGame:setCursor((send ((send gIconBar:curIcon)):cursor)))
    )

	/*
		Modify this method to change any global keyboard bindings.
	*/
    (method (handleEvent pEvent)
        (var theGCursorNumber)
        
        
        (super:handleEvent(pEvent))
        (if ((send pEvent:claimed))
            return 1
        )
        return 
            (switch ((send pEvent:type))
                (case evKEYBOARD
                    (switch ((send pEvent:message))
                        (case KEY_TAB
                            (if (not & (send ((send gIconBar:at(6))):signal) icDISABLED)
                                (if (gFastCast)
                                    return gFastCast
                                )
                                = theGCursorNumber gCursorNumber
                                (send gInv:showSelf(gEgo))
                                (send gGame:setCursor(theGCursorNumber 1))
                                (send pEvent:claimed(TRUE))
                            )
                        )
                        (case KEY_CONTROL
                            (if (not & (send ((send gIconBar:at(7))):signal) icDISABLED)
                                (send gGame:quitGame())
                                (send pEvent:claimed(TRUE))
                            )
                        )
                        (case JOY_RIGHT
                            (if (not & (send ((send gIconBar:at(7))):signal) icDISABLED)
                                = theGCursorNumber (send ((send gIconBar:curIcon)):cursor)
                                (send (ScriptID(24 0)):doit())
                                (send gGameControls:dispose())
                                (send gGame:setCursor(theGCursorNumber 1))
                            )
                        )
                        (case KEY_F2
                            (if ((send gGame:masterVolume()))
                                (send gGame:masterVolume(0))
                            )(else
                                (if (> gPolyphony 1)
                                    (send gGame:masterVolume(15))
                                )(else
                                    (send gGame:masterVolume(1))
                                )
                            )
                            (send pEvent:claimed(TRUE))
                        )
                        (case KEY_F5
                            (if (not & (send ((send gIconBar:at(7))):signal) icDISABLED)
                                (if (gFastCast)
                                    return gFastCast
                                )
                                = theGCursorNumber gCursorNumber
                                (send gGame:save())
                                (send gGame:setCursor(theGCursorNumber 1))
                                (send pEvent:claimed(TRUE))
                            )
                        )
                        (case KEY_F7
                            (if (not & (send ((send gIconBar:at(7))):signal) icDISABLED)
                                (if (gFastCast)
                                    return gFastCast
                                )
                                = theGCursorNumber gCursorNumber
                                (send gGame:restore())
                                (send gGame:setCursor(theGCursorNumber 1))
                                (send pEvent:claimed(TRUE))
                            )
                        )
                        (case KEY_EXECUTE
                            (if ((send gUser:controls))
                                = gGEgoMoveSpeed (send gEgo:moveSpeed)
                                = gGEgoMoveSpeed Max(0 --gGEgoMoveSpeed)
                                (send gEgo:setSpeed(gGEgoMoveSpeed))
                            )
                        )
                        (case KEY_SUBTRACT
                            (if ((send gUser:controls))
                                = gGEgoMoveSpeed (send gEgo:moveSpeed)
                                (send gEgo:setSpeed(++gGEgoMoveSpeed))
                            )
                        )
                        (case 61
                            (if ((send gUser:controls))
                                (send gEgo:setSpeed(6))
                            )
                        )
                        (case KEY_ALT_v
                            (Print:
                                addText("Version number:" 0 0)
                                addText(gVersion 0 14)
                                init()
                            )
                        )
                        (case KEY_ALT_d
                        	// Script-base debugger
                        	(send (ScriptID(INGAME_DEBUG_SCRIPT 0)):init())
                        )
                        (default 
                            (send pEvent:claimed(FALSE))
                        )
                    )
                )
            )
    )


    (method (setCursor cursorNumber param2 param3 param4)
        (var theGCursorNumber)
        = theGCursorNumber gCursorNumber
        (if (paramTotal)
            (if (IsObject(cursorNumber))
                = gCursorNumber cursorNumber
                (send gCursorNumber:init())
            )(else
                = gCursorNumber cursorNumber
                SetCursor(gCursorNumber 0 0)
            )
        )
        (if ((> paramTotal 1) and not param2)
            SetCursor(996 0 0)
        )
        (if (> paramTotal 2)
            SetCursor(param3 param4)
        )
        return theGCursorNumber
    )


    (method (quitGame param1)
        (var temp0, temp1)
        = temp1 (send ((send gIconBar:curIcon)):cursor)
        (send gGame:setCursor(999))
        = temp0 (Print:
                font(gFont)
                width(75)
                mode(1)
                addText(N_QUITMENU V_LOOK 0 1 0 0 0)
                addColorButton(1 N_QUITMENU V_LOOK 0 2 0 25 0)
                addColorButton(0 N_QUITMENU V_LOOK 0 3 0 35 0)
                init()
            )
        (if (temp0)
            (Print:
                addText(19 1 0 4 0 0 0)
                init()
            )
            (super:quitGame(rest param1))
        )(else
            (send gGame:setCursor(temp1))
        )
    )

	/*
		Modify this method to add any default messages for actions.
	*/
    (method (pragmaFail)
        (if ((User:canControl()))
            (switch ((send ((send gUser:curEvent)):message))
                (case V_DO
                    (send gMessager:say(0 V_DO 0 Random(1 2) 0 0))
                )
                (case V_TALK
                    (send gMessager:say(0 V_TALK 0 Random(1 2) 0 0))
                )
                (default 
                    (if (not IsOneOf((send ((send gUser:curEvent)):message) V_LOOK))
                        (send gMessager:say(0 V_COMBINE 0 Random(2 3) 0 0))
                    )
                )
            )
        )
    )

	/*
		This disables player control (e.g. for cutscenes).
	*/
    (method (handsOff)
        (if (not gCurrentIcon)
            = gCurrentIcon (send gIconBar:curIcon)
        )
        = gGUserCanControl (send gUser:canControl())
        = gGUserCanInput (send gUser:canInput())
        (send gUser:
            canControl(0)
            canInput(0)
        )
        (send gEgo:setMotion(0))
        = gCheckedIcons 0
        (send gIconBar:eachElementDo(#perform checkIcon))
        (send gIconBar:curIcon((send gIconBar:at(7))))
        (send gIconBar:disable())
        (send gIconBar:disable(
        		ICONINDEX_WALK
        		ICONINDEX_LOOK
        		ICONINDEX_DO
        		ICONINDEX_TALK
        		ICONINDEX_CUSTOM
        		ICONINDEX_CURITEM
        		ICONINDEX_INVENTORY
        		ICONINDEX_SETTINGS)
        		)
        (send gGame:setCursor(996))
    )

	/*
		This re-enables player control after having been disabled.
	*/
    (method (handsOn fRestore)
        (send gIconBar:enable())
        (send gUser:
            canControl(1)
            canInput(1)
        )
        (send gIconBar:enable(
        		ICONINDEX_WALK
        		ICONINDEX_LOOK
        		ICONINDEX_DO
        		ICONINDEX_TALK
        		// ICONINDEX_CUSTOM // see below
        		ICONINDEX_CURITEM
        		ICONINDEX_INVENTORY
        		ICONINDEX_SETTINGS)
        		)
        		
        (send gIconBar:disable(
        		// See above
        		ICONINDEX_CUSTOM)
		)
        (if (paramTotal and fRestore)
            RestorePreviousHandsOn()
        )
        (if (not (send gIconBar:curInvIcon))
            (send gIconBar:disable(ICONINDEX_CURITEM))
        )
        (if (gCurrentIcon)
            (send gIconBar:curIcon(gCurrentIcon))
            (send gGame:setCursor((send gCurrentIcon:cursor)))
            = gCurrentIcon 0
            (if ((== (send gIconBar:curIcon) (send gIconBar:at(5))) and not (send gIconBar:curInvIcon))
                (send gIconBar:advanceCurIcon())
            )
        )
        (send gGame:setCursor((send ((send gIconBar:curIcon)):cursor) 1))
        = gCursorNumber (send ((send gIconBar:curIcon)):cursor)
    )


    (method (showAbout)
        (send (ScriptID(ABOUT_SCRIPT 0)):doit())
        DisposeScript(ABOUT_SCRIPT)
    )


    (method (showControls)
        (var temp0)
        = temp0 (send ((send gIconBar:curIcon)):cursor)
        (send (ScriptID(GAMECONTROLS_SCRIPT 0)):doit())
        (send gGameControls:dispose())
        (send gGame:setCursor(temp0 1))
    )

)

(instance icon0 of IconItem
    (properties
        view 990
        loop 0
        cel 0
        cursor 980
        type $5000
        message V_WALK
        signal $0041
        maskView 990
        maskLoop 13
        noun N_MOVEICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )

    (method (select params)
        (var temp0)
        return 
            (if ((super:select(rest params)))
                (send gIconBar:hide())
                return 1
            )(else
                return 0
            )
    )

)
(instance icon1 of IconItem
    (properties
        view 990
        loop 1
        cel 0
        cursor 981
        message V_LOOK
        signal $0041
        maskView 990
        maskLoop 13
        noun N_EXAMINEICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )
)

(instance icon2 of IconItem
    (properties
        view 990
        loop 2
        cel 0
        cursor 982
        message V_DO
        signal $0041
        maskView 990
        maskLoop 13
        noun N_DOICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )
)

(instance icon3 of IconItem
    (properties
        view 990
        loop 3
        cel 0
        cursor 983
        message V_TALK
        signal $0041
        maskView 990
        maskLoop 13
        maskCel 4
        noun N_TALKICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )

)

// Use this icon for whatever action you want
(instance icon4 of IconItem
    (properties
        view 990
        loop 10			// This is currently a loop with "empty" cels
        cel 0
        cursor 999		// The cursor view associated with your action.
        message 0		// The verb associated with this action.
        signal $0041
        maskView 990
        maskLoop 13
        maskCel 4
        noun 0			// The noun for your button
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )

)
(instance icon6 of IconItem
    (properties
        view 990
        loop 4
        cel 0
        cursor 999
        message 0
        signal $0041
        maskView 990
        maskLoop 13
        maskCel 4
        noun N_INVENTORYICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )


    (method (select param1)
        (var newEvent, temp1, currentInvIcon, temp3, temp4)
        return 
            (if (& signal icDISABLED)
                0
            )(else
                (if ((paramTotal and param1) and (& signal notUpd))
                    = currentInvIcon (send gIconBar:curInvIcon)
                    (if (currentInvIcon)
                        = temp3 (+ (/ (- (- nsRight nsLeft) CelWide((send currentInvIcon:view) 2 (send currentInvIcon:cel))) 2) nsLeft)
                        = temp4 (+ (+ (send gIconBar:y) (/ (- (- nsBottom nsTop) CelHigh((send currentInvIcon:view) 2 (send currentInvIcon:cel))) 2)) nsTop)
                    )
                    = temp1 1
                    DrawCel(view loop temp1 nsLeft nsTop -1)
                    = currentInvIcon (send gIconBar:curInvIcon)
                    (if (currentInvIcon)
                        DrawCel((send (= currentInvIcon (send gIconBar:curInvIcon)):view) 2 (send currentInvIcon:cel) temp3 temp4 -1)
                    )
                    Graph(grUPDATE_BOX nsTop nsLeft nsBottom nsRight 1)
                    (while (<> (send ((= newEvent (Event:new()))):type) 2)
                        (send newEvent:localize())
                        (if ((self:onMe(newEvent)))
                            (if (not temp1)
                                = temp1 1
                                DrawCel(view loop temp1 nsLeft nsTop -1)
                                = currentInvIcon (send gIconBar:curInvIcon)
                                (if (currentInvIcon)
                                    DrawCel((send (= currentInvIcon (send gIconBar:curInvIcon)):view) 2 (send currentInvIcon:cel) temp3 temp4 -1)
                                )
                                Graph(grUPDATE_BOX nsTop nsLeft nsBottom nsRight 1)
                            )
                        )(else
                            (if (temp1)
                                = temp1 0
                                DrawCel(view loop temp1 nsLeft nsTop -1)
                                = currentInvIcon (send gIconBar:curInvIcon)
                                (if (currentInvIcon)
                                    DrawCel((send (= currentInvIcon (send gIconBar:curInvIcon)):view) 2 (send currentInvIcon:cel) temp3 temp4 -1)
                                )
                                Graph(grUPDATE_BOX nsTop nsLeft nsBottom nsRight 1)
                            )
                        )
                        (send newEvent:dispose())
                    )
                    (send newEvent:dispose())
                    (if (== temp1 1)
                        DrawCel(view loop 0 nsLeft nsTop -1)
                        = currentInvIcon (send gIconBar:curInvIcon)
                        (if (currentInvIcon)
                            DrawCel((send (= currentInvIcon (send gIconBar:curInvIcon)):view) 2 (send currentInvIcon:cel) temp3 temp4 -1)
                        )
                        Graph(grUPDATE_BOX nsTop nsLeft nsBottom nsRight 1)
                    )
                    temp1
                )(else
                    1
                )
            )
    )

)
(instance icon7 of IconItem
    (properties
        view 990
        loop 5
        cel 0
        cursor 999
        type $0000
        message 0
        signal $0043
        maskView 990
        maskLoop 13
        noun N_SELECTINVICON2
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )


    (method (select params)
        (var theGCursorNumber)
        return 
            (if ((super:select(rest params)))
                (send gIconBar:hide())
                = theGCursorNumber gCursorNumber
                (send gInv:showSelf(gEgo))
                (send gGame:setCursor(theGCursorNumber 1))
                return 1
            )(else
                return 0
            )
    )

)
(instance icon8 of IconItem
    (properties
        view 990
        loop 7
        cel 0
        cursor 999
        message V_COMBINE
        signal $0043
        maskView 990
        maskLoop 13
        noun N_SETTINGSICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (super:init())
    )


    (method (select params)
        return 
            (if ((super:select(rest params)))
                (send gIconBar:hide())
                (send gGame:showControls())
                return 1
            )(else
                return 0
            )
    )

)
(instance icon9 of IconItem
    (properties
        view 990
        loop 9
        cel 0
        cursor 989
        type evHELP
        message V_HELP
        signal $0003
        maskView 990
        maskLoop 13
        noun N_HELPICON
        helpVerb V_HELP
    )

    (method (init)
        = lowlightColor gLowlightColor
        (if (gDialog)
            (send gDialog:dispose())
        )
        (super:init())
    )

)
(instance checkIcon of Code
    (properties)

    (method (doit param1)
        (if ((send param1:isKindOf(IconItem)) and (& (send param1:signal) $0004))
            = gCheckedIcons (| gCheckedIcons (>> $8000 (send gIconBar:indexOf(param1))))
        )
    )

)
(instance lb2DoVerbCode of Code
    (properties)

    (method (doit theVerb param2)
        (if ((User:canControl()))
            (if (== param2 gEgo)
                (if (Message(msgSIZE 0 N_EGO theVerb 0 1))
                    (send gMessager:say(N_EGO theVerb 0 0 0 0))
                )(else
                    (send gMessager:say(N_EGO 0 0 Random(1 2) 0 0))
                )
            )(else
                (switch (theVerb)
                    (case V_DO
                        (send gMessager:say(0 V_DO 0 Random(1 2) 0 0))
                    )
                    (case V_TALK
                        (send gMessager:say(0 V_TALK 0 Random(1 2) 0 0))
                    )
                    (default 
                        (if (not IsOneOf(theVerb V_LOOK))
                            (send gMessager:say(0 V_COMBINE 0 Random(2 3) 0 0))
                        )
                    )
                )
            )
        )
    )
)

(instance lb2FtrInit of Code
    (properties)

    (method (doit param1)
        (if (== (send param1:sightAngle) $6789)
            (send param1:sightAngle(90))
        )
        (if (== (send param1:actions) $6789)
            (send param1:actions(0))
        )
        (if (not (send param1:approachX) and not (send param1:approachY))
            (send param1:
                approachX((send param1:x))
                approachY((send param1:y))
            )
        )
    )
)

// This converts verbs into a bit flag mask
(instance lb2ApproachCode of Code
    (properties)

    (method (doit param1)
        (switch (param1)
            (case V_LOOK
                1
            )
            (case V_TALK
                2
            )
            (case V_WALK
                4
            )
            (case V_DO
                8
            )
            // Add other verbs here, with doubling numbers.
            /*
            (31
                16
            )
            (29
                64
            )
            (25
                128
            )*/
            (default 
                $8000
            )
        )
    )
)

(instance mainWindow of BorderWindow
    (properties)
)
(instance templateNarrator of Narrator
    (properties)

    (method (init param1)
        = font gFont
        (self:back(gColorWindowBackground))
        (super:init(rest param1))
    )

)
(instance testMessager of Messager
    (properties)

    (method (findTalker talkerNumber)
        (var temp0)
        = gCurrentTalkerNumber talkerNumber
        = temp0 
            (switch (talkerNumber)
                (case NARRATOR
                    gNarrator
                )
                // Add more cases here for different narrators
                // (8
                    // (ScriptID 109 7)
                //)
            )
            
        (if (temp0)
        	(if (not (send temp0:isKindOf(Narrator)))
        		Prints("Invalid talker.")
			)
            return temp0
        )(else
            return (super:findTalker(talkerNumber))
        )
    )

)
