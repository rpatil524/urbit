::                                                      ::  ::
::::  /hoon/hood/app                                    ::  ::
  ::                                                    ::  ::
/?    310                                               ::  zuse version
/-  *sole
/+  sole,                                               ::  libraries
    ::  XX these should really be separate apps, as
    ::     none of them interact with each other in
    ::     any fashion; however, to reduce boot-time
    ::     complexity and work around the current
    ::     non-functionality of end-to-end acknowledgments,
    ::     they have been bundled into :hood
    ::
    ::  |command handlers
    hood-helm-mall, hood-kiln-mall, hood-drum-mall, hood-write-mall
::                                                      ::  ::
::::                                                    ::  ::
  ::                                                    ::  ::
|%
++  hood-module
  ::  each hood module follows this general shape
  =>  |%
      +$  part  [%module %0 pith]
      +$  pith  ~
      ++  take
        |~  [wire sign-arvo]
        *(quip card:agent:mall part)
      ++  take-agent
        |~  [wire gift:agent:mall]
        *(quip card:agent:mall part)
      ++  poke
        |~  [mark vase]
        *(quip card:agent:mall part)
      --
  |=  [bowl:mall own=part]
  |_  moz=(list card:agent:mall)
  ++  abet  [(flop moz) own]
  --
--
::                                                      ::  ::
::::                                                    ::  ::  state handling
  ::                                                    ::  ::
!:
=>  |%                                                  ::
    ++  hood-old                                        ::  unified old-state
      {?($0 $1) lac/(map @tas hood-part-old)}           ::
    ++  hood-1                                          ::  unified state
      {$1 lac/(map @tas hood-part)}                     ::
    ++  hood-good                                       ::  extract specific
      =+  hed=$:hood-head
      |@  ++  $
            |:  paw=$:hood-part
            ?-  hed
              $drum  ?>(?=($drum -.paw) `part:hood-drum-mall`paw)
              $helm  ?>(?=($helm -.paw) `part:hood-helm-mall`paw)
              $kiln  ?>(?=($kiln -.paw) `part:hood-kiln-mall`paw)
              $write  ?>(?=($write -.paw) `part:hood-write-mall`paw)
            ==
      --
    ++  hood-head  _-:$:hood-part                       ::  initialize state
    ++  hood-make                                       ::
      =+  $:{our/@p hed/hood-head}                      ::
      |@  ++  $
            ?-  hed
              $drum  (make:hood-drum-mall our)
              $helm  *part:hood-helm-mall
              $kiln  *part:hood-kiln-mall
              $write  *part:hood-write-mall
            ==
      --
    ++  hood-part-old  hood-part                        ::  old state for ++prep
    ++  hood-port                                       ::  state transition
      |:  paw=$:hood-part-old  ^-  hood-part            ::
      paw                                               ::
    ::                                                  ::
    ++  hood-part                                       ::  current module state
      $%  {$drum $2 pith-2:hood-drum-mall}              ::
          {$helm $0 pith:hood-helm-mall}                ::
          {$kiln $0 pith:hood-kiln-mall}                ::
          {$write $0 pith:hood-write-mall}              ::
      ==                                                ::
    --                                                  ::
::                                                      ::  ::
::::                                                    ::  ::  app proper
  ::                                                    ::  ::
^-  agent:mall
=|  hood-1                                              ::  module states
=>  |%
    ++  help
    |=  hid/bowl:mall
      |%
      ++  able                                          ::  find+make part
        =+  hed=$:hood-head
        |@  ++  $
              =+  rep=(~(get by lac) hed)
              =+  par=?^(rep u.rep `hood-part`(hood-make our.hid hed))
              ((hood-good hed) par)
        --
      ::
      ++  ably                                          ::  save part
        =+  $:{(list) hood-part}
        |@  ++  $
              [+<- (~(put by lac) +<+< +<+)]
        --
      ::                                                ::  ::
      ::::                                              ::  ::  generic handling
        ::                                              ::  ::
      ++  prep
        |=  old/(unit hood-old)  ^-  (quip _!! _+>)
        :-  ~
        ?~  old  +>
        +>(lac (~(run by lac.u.old) hood-port))
      ::
      ++  poke-hood-load                                ::  recover lost brain
        |=  dat/hood-part
        ?>  =(our.hid src.hid)
        ~&  loaded+-.dat
        [~ (~(put by lac) -.dat dat)]
      ::
      ::
      ++  from-module                                   ::  create wrapper
        |*  _[identity=%module start=..$ finish=_abet]:(hood-module)
        =-  [wrap=- *start]                 ::  usage (wrap handle-arm):from-foo
        |*  handle/_finish
        |=  a=_+<.handle
        =.  +>.handle  (start hid (able identity))
        ^-  (quip card:agent:mall _lac)
        %-  ably
        ^-  (quip card:agent:mall hood-part)
        (handle a)
      ::  per-module interface wrappers
      ++  from-drum  (from-module %drum [..$ _se-abet]:(hood-drum-mall))
      ++  from-helm  (from-module %helm [..$ _abet]:(hood-helm-mall))
      ++  from-kiln  (from-module %kiln [..$ _abet]:(hood-kiln-mall))
      ++  from-write  (from-module %write [..$ _abet]:(hood-write-mall))
      --
    --
|_  hid/bowl:mall                                       ::  gall environment
++  handle-init
  `..handle-init
::
++  handle-extract-state
  !>([%1 lac])
::
++  handle-upgrade-state
  |=  =old-state=vase
  =/  old-state  !<(hood-1 old-state-vase)
  ?~  old-state
    ~&  %prep-lost
    `..handle-init
  ~&  %prep-found
  `..handle-init(lac lac.u.old-state)
::
++  handle-poke
  |=  [=mark =vase]
  ^-  (quip card:agent:mall agent:mall)
  =/  h  (help hid)
  =^  cards  lac
    ?:  =(%helm (end 3 4 mark))
      ((wrap poke):from-helm:h mark vase)
    ?:  =(%drum (end 3 4 mark))
      ((wrap poke):from-drum:h mark vase)
    ?:  =(%kiln (end 3 4 mark))
      ((wrap poke):from-kiln:h mark vase)
    ?:  =(%write (end 3 5 mark))
      ((wrap poke):from-write:h mark vase)
    ::  XX should rename and move to libs
    ::
    ?+  mark  ~|([%poke-hood-bad-mark mark] !!)
      %hood-load  %-  poke-hood-load:h
                  (need !<(hood-part vase))
      %atom       %-  (wrap poke-atom):from-helm:h
                  (need !<(@ vase))
      %dill-belt  %-  (wrap poke-dill-belt):from-drum:h
                  (need !<(dill-belt:dill vase))
      %dill-blit  %-  (wrap poke-dill-blit):from-drum:h
                  (need !<(dill-blit:dill vase))
      %hood-sync  %-  (wrap poke-sync):from-kiln:h
                  (need !<([desk ship desk] vase))
    ==
  [cards ..handle-init]
::
++  handle-subscribe
  |=  =path
  =/  h  (help hid)
  =^  cards  lac
    ?+  path  ~|([%hood-bad-path wire] !!)
      [%drum *]  ((wrap peer):from-drum:h t.path)
    ==
  [cards ..handle-init]
::
++  handle-unsubscribe
  |=  path
  `..handle-init
::
++  handle-peek
  |=  path
  *(unit (unit cage))
::
++  handle-agent-response
  |=  [=wire =gift:agent:mall]
  =/  h  (help hid)
  =^  cards  lac
    ?+  wire  ~|([%hood-bad-wire wire] !!)
      [%helm *]   ((wrap take-agent):from-helm:h wire gift)
      [%kiln *]   ((wrap take-agent):from-kiln:h wire gift)
      [%drum *]   ((wrap take-agent):from-drum:h wire gift)
      [%write *]  ((wrap take-agent):from-write:h wire gift)
    ==
  [cards ..handle-init]
::
++  handle-arvo-response
  |=  [=wire =sign-arvo]
  =/  h  (help hid)
  =^  cards  lac
    ?+  wire  ~|([%hood-bad-wire wire] !!)
      [%helm *]   ((wrap take):from-helm:h t.wire sign-arvo)
      [%drum *]   ((wrap take):from-drum:h t.wire sign-arvo)
      [%kiln *]   ((wrap take-general):from-kiln:h t.wire sign-arvo)
      [%write *]  ((wrap take):from-write:h t.wire sign-arvo)
    ==
  [cards ..handle-init]
::
++  handle-error
  |=  [term tang]
  `..handle-init
--
