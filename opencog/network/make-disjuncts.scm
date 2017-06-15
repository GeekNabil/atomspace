;
; make-disjuncts.scm
;
; Compute the disjuncts, obtained from an MST parse of a sequence.
;
; Copyright (c) 2017 Linas Vepstas
;
; ---------------------------------------------------------------------
; OVERVIEW
; --------
; After a sequence of atoms has been parsed with the MST parser, the
; links between atoms in the parse can be interpreted as Link Grammar
; links (connector pairs).  The connector pair is the labelled edge
; between the two atoms; the label itself is is given by the names of
; the two endpoints. A single connector is then just a direction (to
; the left, to the right) plus the vertex atom at the far end.
;
; The connector set is then a sequence of conectors; the number of
; connectors in the set exactly equal to the degree of the vertex in the
; MST parse: the connector set "describes" the parse tree, locally.
;
; An example of a connector set is then
;
;    CSet
;        Atom "something"
;        ConnectorSeq
;            Connector
;                Atom "its"
;                ConnectorDir "-"
;            Connector
;                Atom "curious"
;                ConnectorDir "+"
;
; which captures the idea that 
;
; ---------------------------------------------------------------------

(use-modules (srfi srfi-1))

; ---------------------------------------------------------------------

(define-public (make-pseudo-disjuncts MST-PARSE)
"
  make-pseudo-disjuncts - create 'decoded' disjuncts.

  Given an MST parse of a sentence, return a list of 'decoded'
  disjuncts for each word in the sentence.

  It is the nature of MST parses that the links between the words
  have no labels: the links are of the 'any' type. We'd like to
  disover thier types, and we begin by creating pseudo-disjuncts.
  These resemble ordinary disjuncts, except that the connectors
  are replaced by the words that they connect to.

  So, for example, given the MST parse
     (mst-parse-text 'The game is played on a level playing field')
  the word 'playing' might get this connector set:

    (PseudoWordCset
       (WordNode \"playing\")
       (PseudoAnd
          (PseudoConnector
             (WordNode \"level\")
             (LgConnDirNode \"-\"))
          (PseudoConnector
             (WordNode \"field\")
             (LgConnDirNode \"+\"))))

  Grammatically-speaking, this is not a good connector, but it does
  show the general idea: that there was a link level<-->playing and
  a link playing<-->field.
"
	; Discard links with bad MI values; anything less than
	; -50 is bad. Heck, anything under minus ten...
	(define good-links (filter
		(lambda (mlink) (< -50 (mst-link-get-score mlink)))
		MST-PARSE))

	; Create a list of all of the words in the sentence.
	(define seq-list (delete-duplicates!
		(fold
			(lambda (mlnk lst)
				(cons (mst-link-get-left-numa mlnk)
					(cons (mst-link-get-right-numa mlnk) lst)))
			'()
			good-links)))

	; Return #t if word appears on the left side of mst-lnk
	(define (is-on-left-side? wrd mlnk)
		(equal? wrd (mst-link-get-left-atom mlnk)))
	(define (is-on-right-side? wrd mlnk)
		(equal? wrd (mst-link-get-right-atom mlnk)))

	; Given a word, and the mst-parse linkset, create a shorter
	; seq-list which holds only the words linked to the right.
	(define (mk-right-seqlist seq mparse)
		(define wrd (mst-numa-get-atom seq))
		(map mst-link-get-right-numa
			(filter
				(lambda (mlnk) (is-on-left-side? wrd mlnk))
				mparse)))

	(define (mk-left-seqlist seq mparse)
		(define wrd (mst-numa-get-atom seq))
		(map mst-link-get-left-numa
			(filter
				(lambda (mlnk) (is-on-right-side? wrd mlnk))
				mparse)))

	; Sort a seq-list into ascending order
	(define (sort-seqlist seq-list)
		(sort seq-list
			(lambda (sa sb)
				(< (mst-numa-get-index sa) (mst-numa-get-index sb)))))

	; Given a word, the the links, create a pseudo-disjunct
	(define (mk-pseudo seq mlist)
		(define lefts (sort-seqlist (mk-left-seqlist seq mlist)))
		(define rights (sort-seqlist (mk-right-seqlist seq mlist)))

		; Create a list of left-connectors
		(define left-cnc
			(map (lambda (sw)
					(PseudoConnector
						(mst-numa-get-atom sw)
						(LgConnDirNode "-")))
			lefts))

		(define right-cnc
			(map (lambda (sw)
					(PseudoConnector
						(mst-numa-get-atom sw)
						(LgConnDirNode "+")))
			rights))

		; return the connector-set
		(PseudoWordCset
			(mst-numa-get-atom seq)
			(PseudoAnd (append left-cnc right-cnc)))
	)

	(map
		(lambda (seq) (mk-pseudo seq MST-PARSE))
		seq-list)
)

;  ---------------------------------------------------------------------
