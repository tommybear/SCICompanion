.. StopWalk

.. default - domain::js

.. include:: /includes/standard.rst

==============================
StopWalk (of :class:`Forward`)
==============================

.. class:: StopWalk

	Defined in StopWalk.sc.

	
	StopWalk is a cycler that switches to a different loop or a different view
	when the :class:`Actor` stops moving. If no separate view is specified, the
	"stopped" state uses the last loop of the current Actor view.
	
	Example usage::
	
		(john setCycle: StopWalk -1)


.. blockdiag::
	:alt: class diagram
	:width: 600

	diagram {
		default_fontsize = 16
		Cycle -> MouthSync
		Cycle -> Smopper
		Cycle -> MoveCycle
		Cycle -> FlickerCycler
		Cycle -> Forward
		Cycle -> Blink
		Cycle -> RangeOscillate
		Cycle -> Oscillate
		Cycle -> RandCycle
		Cycle -> Reverse
		Cycle -> GradualCycler
		Cycle -> CycleTo
		CycleTo -> EndLoop
		CycleTo -> BegLoop
		Forward -> ForwardCounter
		Forward -> StopWalk
		Forward -> Walk
		Smopper -> FiddleStopWalk
		StopWalk [color=greenyellow]
	}

Properties
==========

Inherited from :class:`Forward`:

========= ===========
Property  Description
========= ===========
client               
caller               
cycleDir             
cycleCnt             
completed            
name                 
========= ===========

Defined in StopWalk:

======== ================
Property Description     
======== ================
vWalking The walking view
vStopped The stopped view
======== ================


Methods
==========


.. function:: init(theClient theVStopped)
	:noindex:

	:param heapPtr theClient: The Actor to which this applies.
	:param number theVStopped: The number of the stopped view, or -1 if the last loop of the current view is to be used.



.. function:: doit()
	:noindex:



.. function:: dispose()
	:noindex:



