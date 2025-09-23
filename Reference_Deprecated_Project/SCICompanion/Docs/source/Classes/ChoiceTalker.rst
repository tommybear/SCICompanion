.. ChoiceTalker

.. default - domain::js

.. include:: /includes/standard.rst

=================================
ChoiceTalker (of :class:`Talker`)
=================================

.. class:: ChoiceTalker

	Defined in ChoiceTalker.sc.

	
	ChoiceTalker is a subclass of :class:`Talker` that display a series of options
	for the player to choose. Given a noun/verb/condition tuple, sequence 1 will be
	the title, and the following sequences will be the options. The number of the option
	chosen will be the sequence number of that choice minus one.
	
	To put a ChoiceTalker in *choice mode* do::
	
		(myChoiceTalker
			normal: FALSE
			curNoun: noun
			curVerb: verb
			curCase: condition
		)
	
	Then have it say something, going through the gMessager like usual, and passing the
	same noun/verb/condition and 0 for the sequence::
	
		(gMessager say: noun verb condition 0)
	
	Then you can check the whichSelect property::
	
		(switch (myChoiceTalker whichSelect?)
			(1
				; Do something
			)
			(2
				; Do something else
			)
		)


.. blockdiag::
	:alt: class diagram
	:width: 600

	diagram {
		default_fontsize = 16
		orientation = portrait;
		Feature -> View
		View -> Prop
		Prop -> Narrator
		Prop -> Actor
		Prop -> Door
		Actor -> Ego
		Ego -> GameEgo
		Narrator -> Talker
		Narrator -> ChoiceNarrator
		Talker -> PriorityTalker
		Talker -> ChoiceTalker
		ChoiceTalker [color=greenyellow]
	}

Properties
==========

Inherited from :class:`Talker`:

=============== ===========
Property        Description
=============== ===========
x                          
y                          
z                          
heading                    
noun                       
case                      
modNum                     
nsTop                      
nsLeft                     
nsBottom                   
nsRight                    
sightAngle                 
actions                    
onMeCheck                  
state                      
approachX                  
approachY                  
approachDist               
_approachVerbs             
yStep                      
view                       
loop                       
cel                        
priority                   
underBits                  
signal                     
lsTop                      
lsLeft                     
lsBottom                   
lsRight                    
brTop                      
brLeft                     
brBottom                   
brRight                    
scaleSignal                
scaleX                     
scaleY                     
maxScale                   
cycleSpeed                 
script                     
cycler                     
timer                      
detailLevel                
scaler                     
caller                     
disposeWhenDone            
ticks                      
talkWidth                  
keepWindow                 
modeless                   
font                       
cueVal                     
initialized                
showTitle                  
color                      
back                       
curVolume                  
saveCursor                 
bust                       
eyes                       
mouth                      
viewInPrint                
textX                      
textY                      
useFrame                   
blinkSpeed                 
name                       
=============== ===========

Defined in ChoiceTalker:

=========== ====================================================
Property    Description                                         
=========== ====================================================
verb                                                            
case                                                            
whichSelect The sequence number that was selected by the player.
normal      If FALSE, the Talker offers options.                
curNoun                                                         
curVerb                                                         
curCase                                                         
=========== ====================================================


Methods
==========

.. function:: show(param1)
	:noindex:



.. function:: say(param1 param2 params)
	:noindex:



.. function:: startText(param1 param2)
	:noindex:



.. function:: display(theText param2)
	:noindex:



.. function:: cycle(param1 param2)
	:noindex:



