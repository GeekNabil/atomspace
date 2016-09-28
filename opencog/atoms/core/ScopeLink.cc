
/*
 * ScopeLink.cc
 *
 * Copyright (C) 2009, 2014, 2015 Linas Vepstas
 *
 * Author: Linas Vepstas <linasvepstas@gmail.com>  January 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the
 * exceptions at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <opencog/atoms/base/ClassServer.h>
#include <opencog/atoms/TypeNode.h>
#include <opencog/atoms/core/FreeLink.h>
#include <opencog/atoms/core/LambdaLink.h>

#include "ScopeLink.h"

using namespace opencog;

void ScopeLink::init(void)
{
	extract_variables(_outgoing);
}

ScopeLink::ScopeLink(const HandleSeq& oset,
                     TruthValuePtr tv, AttentionValuePtr av)
	: Link(SCOPE_LINK, oset, tv, av)
{
	init();
}

ScopeLink::ScopeLink(const Handle& vars, const Handle& body,
                     TruthValuePtr tv, AttentionValuePtr av)
	: Link(SCOPE_LINK, HandleSeq({vars, body}), tv, av)
{
	init();
}

ScopeLink::ScopeLink(Type t, const Handle& body,
                     TruthValuePtr tv, AttentionValuePtr av)
	: Link(t, HandleSeq({body}), tv, av)
{
	// Derived classes have a different initialization sequence
	if (SCOPE_LINK != t) return;
	init();
}

ScopeLink::ScopeLink(Type t, const HandleSeq& oset,
                     TruthValuePtr tv, AttentionValuePtr av)
	: Link(t, oset, tv, av)
{
	// Derived classes have a different initialization sequence
	if (SCOPE_LINK != t) return;
	init();
}

ScopeLink::ScopeLink(Link &l)
	: Link(l)
{
	// Type must be as expected.
	Type tscope = l.getType();
	if (not classserver().isA(tscope, SCOPE_LINK))
	{
		const std::string& tname = classserver().getTypeName(tscope);
		throw InvalidParamException(TRACE_INFO,
			"Expecting a ScopeLink, got %s", tname.c_str());
	}

	// Derived types have a different initialization sequence.
	if (SCOPE_LINK != tscope) return;
	init();
}

/* ================================================================= */
///
/// Find and unpack variable declarations, if any; otherwise, just
/// find all free variables.
///
void ScopeLink::extract_variables(const HandleSeq& oset)
{
	if (oset.size() == 0)
		throw SyntaxException(TRACE_INFO,
			"Expecting a non-empty outgoing set.");

	Type decls = oset.at(0)->getType();

	// If the first atom is not explicitly a variable declaration, then
	// there are no variable declarations. There are two cases that; can
	// apply here: either the body is a lambda, in which case, we copy
	// the variables from the lambda; else we extract all free variables.
	if (VARIABLE_LIST != decls and
	    VARIABLE_NODE != decls and
	    TYPED_VARIABLE_LINK != decls and
	    GLOB_NODE != decls)
	{
		_body = oset[0];

		if (classserver().isA(_body->getType(), LAMBDA_LINK))
		{
			LambdaLinkPtr lam(LambdaLinkCast(_body));
			if (nullptr == lam)
				lam = createLambdaLink(*LinkCast(_body));
			_varlist = lam->get_variables();
			_body = lam->get_body();
		}
		else
		{
			_varlist.find_variables(oset[0]);
		}
		return;
	}

	if (oset.size() < 2)
		throw SyntaxException(TRACE_INFO,
			"Expecting an outgoing set size of at least two; got %s",
			oset[0]->toString().c_str());

	// If we are here, then the first outgoing set member should be
	// a variable declaration.
	_vardecl = oset[0];
	_body = oset[1];

	// Initialize _varlist with the scoped variables
	init_scoped_variables(_vardecl);
}

/* ================================================================= */
///
/// Initialize _varlist given a handle of either VariableList or a
/// variable.
///
void ScopeLink::init_scoped_variables(const Handle& hvar)
{
	// Use the VariableList class as a tool to extract the variables
	// for us.
	VariableList vl(hvar);
	_varlist = vl.get_variables();
}

/* ================================================================= */
///
/// Compare other ScopeLink, return true if it is equal to this one,
/// up to an alpha-conversion of variables.
///
bool ScopeLink::is_equal(const Handle& other) const
{
	if (other == this) return true;
	if (other->getType() != _type) return false;

	ScopeLinkPtr scother(ScopeLinkCast(other));

	// In case we're dealing with a class inheriting from ScopeLink,
	// like BindLink, that has more than one scoped terms, like
	// implicand, etc, then we need to check the alpha equivalence
	// over all terms. Before that let's check that they have the same
	// number of terms.
	Arity vardecl_offset = _vardecl != Handle::UNDEFINED;
	Arity other_vardecl_offset = scother->_vardecl != Handle::UNDEFINED;
	Arity n_scoped_terms = getArity() - vardecl_offset;
	Arity other_n_scoped_terms = other->getArity() - other_vardecl_offset;
	if (n_scoped_terms != other_n_scoped_terms) return false;

	// Variable declarations must match.
	if (not _varlist.is_equal(scother->_varlist)) return false;

	// Other terms, with our variables in place of its variables,
	// should be same as our terms.
	for (Arity i = 0; i < n_scoped_terms; ++i) {
		Handle h = getOutgoingAtom(i + vardecl_offset);
		Handle other_h = other->getOutgoingAtom(i + other_vardecl_offset);
		other_h = scother->_varlist.substitute_nocheck(other_h, _varlist.varseq);
 		// Compare them, they should match.
 		if (*((AtomPtr)h) != *((AtomPtr) other_h)) return false;
 	}

	return true;
}

Handle ScopeLink::alpha_conversion() const
{
	// TODO
	return Handle::UNDEFINED;
}

bool ScopeLink::operator==(const Atom& ac) const
{
	Atom& a = (Atom&) ac; // cast away constness, for smart ptr.
	return is_equal(a.getHandle());
}

bool ScopeLink::operator!=(const Atom& a) const
{
	return not operator==(a);
}

/* ===================== END OF FILE ===================== */
