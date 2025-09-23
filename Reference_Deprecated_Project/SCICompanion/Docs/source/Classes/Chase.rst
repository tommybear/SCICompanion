.. Chase

.. default - domain::js

.. include:: /includes/standard.rst

==========================
Chase (of :class:`Motion`)
==========================

.. class:: Chase

	Defined in Chase.sc.

	
	A motion class that is used to make an Actor move until it is within a certain distance of another (possibly moving) object. Then it stops.
	
	Example usage::	
	
		; Make the dog get within 20 pixels of the ego.
		(aDog setMotion: Chase gEgo 20)
	
	Note that this class doesn't intelligently move the Actor around polygon obstacles. For that, use :class:`PChase`.


.. blockdiag::
	:alt: class diagram
	:width: 600

	diagram {
		default_fontsize = 16
		Motion -> Wander
		Motion -> DPath
		Motion -> MoveTo
		Motion -> Approach
		Motion -> Orbit
		Motion -> Follow
		Motion -> PolyPath
		Motion -> Track
		Motion -> Chase
		Motion -> Jump
		Jump -> JumpTo
		PolyPath -> PChase
		PolyPath -> PFollow
		PolyPath -> MoveFwd
		MoveTo -> RegionPath
		DPath -> RelDPath
		Chase [color=greenyellow]
	}

Properties
==========

Inherited from :class:`Motion`:

========= =============================================================
Property  Description                                                  
========= =============================================================
client    The :class:`Actor` to which this is attached.                
caller    The object that will get cue()'d when the motion is complete.
x                                                                      
y                                                                      
dx                                                                     
dy                                                                     
b-moveCnt                                                              
b-i1                                                                   
b-i2                                                                   
b-di                                                                   
b-xAxis                                                                
b-incr                                                                 
completed                                                              
xLast                                                                  
yLast                                                                  
name                                                                   
========= =============================================================

Defined in Chase:

======== ===========
Property Description
======== ===========
who                 
distance            
======== ===========


Methods
==========


.. function:: init(theClient theWho [theDistance theCaller])
	:noindex:

	Initializes the Chase instance.

	:param heapPtr theClient: The :class:`Actor` to which this is attached.
	:param heapPtr theWho: The target to chase.
	:param number theDistance: How close the client needs to get from the target.
	:param heapPtr theCaller: The object on which cue() will be called when the target is reached.



.. function:: doit()
	:noindex:



.. function:: setTarget(param1)
	:noindex:



.. function:: onTarget()
	:noindex:



