// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"

#include "selection.hpp"
#include "cmdline.h"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class SelectionTests
{
    TEST_CLASS(SelectionTests);

    CommonState* m_state;
    Selection* m_pSelection;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();

        m_pSelection = &Selection::Instance();
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_pSelection = nullptr;

        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        return true;
    }

    void VerifyGetSelectionRects_BoxMode()
    {
        const auto selectionRects = m_pSelection->GetSelectionRects();
        const UINT cRectanglesExpected = m_pSelection->_srSelectionRect.Bottom - m_pSelection->_srSelectionRect.Top + 1;

        if (VERIFY_ARE_EQUAL(cRectanglesExpected, selectionRects.size()))
        {
            for (auto iRect = 0; iRect < gsl::narrow<int>(selectionRects.size()); iRect++)
            {
                // ensure each rectangle is exactly the width requested (block selection)
                const auto psrRect = &selectionRects[iRect];

                const auto sRectangleLineNumber = (til::CoordType)iRect + m_pSelection->_srSelectionRect.Top;

                VERIFY_ARE_EQUAL(psrRect->Top, sRectangleLineNumber);
                VERIFY_ARE_EQUAL(psrRect->Bottom, sRectangleLineNumber);

                VERIFY_ARE_EQUAL(psrRect->Left, m_pSelection->_srSelectionRect.Left);
                VERIFY_ARE_EQUAL(psrRect->Right, m_pSelection->_srSelectionRect.Right);
            }
        }
    }

    TEST_METHOD(TestGetSelectionRects_BoxMode)
    {
        m_pSelection->_fSelectionVisible = true;

        // set selection region
        m_pSelection->_srSelectionRect.Top = 0;
        m_pSelection->_srSelectionRect.Bottom = 3;
        m_pSelection->_srSelectionRect.Left = 1;
        m_pSelection->_srSelectionRect.Right = 10;

        // #1 top-left to bottom right selection first
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Left;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Top;

        // A. false/false for the selection modes should mean box selection
        m_pSelection->_fLineSelection = false;
        m_pSelection->_fUseAlternateSelection = false;

        VerifyGetSelectionRects_BoxMode();

        // B. true/true for the selection modes should also mean box selection
        m_pSelection->_fLineSelection = true;
        m_pSelection->_fUseAlternateSelection = true;

        VerifyGetSelectionRects_BoxMode();

        // now try the other 3 configurations of box region.
        // #2 top-right to bottom-left selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Right;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Top;

        VerifyGetSelectionRects_BoxMode();

        // #3 bottom-left to top-right selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Left;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Bottom;

        VerifyGetSelectionRects_BoxMode();

        // #4 bottom-right to top-left selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Right;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Bottom;

        VerifyGetSelectionRects_BoxMode();
    }

    void VerifyGetSelectionRects_LineMode()
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        const auto selectionRects = m_pSelection->GetSelectionRects();
        const UINT cRectanglesExpected = m_pSelection->_srSelectionRect.Bottom - m_pSelection->_srSelectionRect.Top + 1;

        if (VERIFY_ARE_EQUAL(cRectanglesExpected, selectionRects.size()))
        {
            // RULES:
            // 1. If we're only selection one line, select the entire region between the two rectangles.
            //    Else if we're selecting multiple lines...
            // 2. Extend all lines except the last line to the right edge of the screen
            //    Extend all lines except the first line to the left edge of the screen
            // 3. If our anchor is in the top-right or bottom-left corner of the rectangle...
            //    The inside portion of our rectangle on the first and last lines is invalid.
            //    Remove from selection (but preserve the anchors themselves).

            // RULE #1: If 1 line, entire region selected.
            auto fHaveOneLine = selectionRects.size() == 1;

            if (fHaveOneLine)
            {
                auto srSelectionRect = m_pSelection->_srSelectionRect;
                VERIFY_ARE_EQUAL(srSelectionRect.Top, srSelectionRect.Bottom);

                const auto psrRect = &selectionRects[0];

                VERIFY_ARE_EQUAL(psrRect->Top, srSelectionRect.Top);
                VERIFY_ARE_EQUAL(psrRect->Bottom, srSelectionRect.Bottom);

                VERIFY_ARE_EQUAL(psrRect->Left, srSelectionRect.Left);
                VERIFY_ARE_EQUAL(psrRect->Right, srSelectionRect.Right);
            }
            else
            {
                // RULE #2 : Check extension to edges
                for (UINT iRect = 0; iRect < selectionRects.size(); iRect++)
                {
                    // ensure each rectangle is exactly the width requested (block selection)
                    const auto psrRect = &selectionRects[iRect];

                    const auto sRectangleLineNumber = (til::CoordType)iRect + m_pSelection->_srSelectionRect.Top;

                    VERIFY_ARE_EQUAL(psrRect->Top, sRectangleLineNumber);
                    VERIFY_ARE_EQUAL(psrRect->Bottom, sRectangleLineNumber);

                    auto fIsFirstLine = iRect == 0;
                    auto fIsLastLine = iRect == selectionRects.size() - 1;

                    // for all lines except the last, the line should reach the right edge of the buffer
                    if (!fIsLastLine)
                    {
                        // buffer size = 80, then selection goes 0 to 79. Thus X - 1.
                        VERIFY_ARE_EQUAL(psrRect->Right, gci.GetActiveOutputBuffer().GetTextBuffer().GetSize().RightInclusive());
                    }

                    // for all lines except the first, the line should reach the left edge of the buffer
                    if (!fIsFirstLine)
                    {
                        VERIFY_ARE_EQUAL(psrRect->Left, 0);
                    }
                }

                // RULE #3: Check first and last line have invalid regions removed, if applicable
                UINT iFirst = 0;
                auto iLast = gsl::narrow<UINT>(selectionRects.size() - 1u);

                const auto psrFirst = &selectionRects[iFirst];
                const auto psrLast = &selectionRects[iLast];

                auto fRemoveRegion = false;

                auto srSelectionRect = m_pSelection->_srSelectionRect;
                auto coordAnchor = m_pSelection->_coordSelectionAnchor;

                // if the anchor is in the top right or bottom left corner, we must have removed a region. otherwise, it stays as is.
                if (coordAnchor.Y == srSelectionRect.Top && coordAnchor.X == srSelectionRect.Right)
                {
                    fRemoveRegion = true;
                }
                else if (coordAnchor.Y == srSelectionRect.Bottom && coordAnchor.X == srSelectionRect.Left)
                {
                    fRemoveRegion = true;
                }

                // now check the first row's left based on removal
                if (!fRemoveRegion)
                {
                    VERIFY_ARE_EQUAL(psrFirst->Left, srSelectionRect.Left);
                }
                else
                {
                    VERIFY_ARE_EQUAL(psrFirst->Left, srSelectionRect.Right);
                }

                // and the last row's right based on removal
                if (!fRemoveRegion)
                {
                    VERIFY_ARE_EQUAL(psrLast->Right, srSelectionRect.Right);
                }
                else
                {
                    VERIFY_ARE_EQUAL(psrLast->Right, srSelectionRect.Left);
                }
            }
        }
    }

    TEST_METHOD(TestGetSelectionRects_LineMode)
    {
        m_pSelection->_fSelectionVisible = true;

        // Part I: Multiple line selection
        // set selection region
        m_pSelection->_srSelectionRect.Top = 0;
        m_pSelection->_srSelectionRect.Bottom = 3;
        m_pSelection->_srSelectionRect.Left = 1;
        m_pSelection->_srSelectionRect.Right = 10;

        // #1 top-left to bottom right selection first
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Left;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Top;

        // A. true/false for the selection modes should mean line selection
        m_pSelection->_fLineSelection = true;
        m_pSelection->_fUseAlternateSelection = false;

        VerifyGetSelectionRects_LineMode();

        // B. false/true for the selection modes should also mean line selection
        m_pSelection->_fLineSelection = false;
        m_pSelection->_fUseAlternateSelection = true;

        VerifyGetSelectionRects_LineMode();

        // now try the other 3 configurations of box region.
        // #2 top-right to bottom-left selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Right;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Top;

        VerifyGetSelectionRects_LineMode();

        // #3 bottom-left to top-right selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Left;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Bottom;

        VerifyGetSelectionRects_LineMode();

        // #4 bottom-right to top-left selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Right;
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Bottom;

        VerifyGetSelectionRects_LineMode();

        // Part II: Single line selection
        m_pSelection->_srSelectionRect.Top = 2;
        m_pSelection->_srSelectionRect.Bottom = 2;
        m_pSelection->_srSelectionRect.Left = 1;
        m_pSelection->_srSelectionRect.Right = 10;

        // #1: left to right selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Left;
        VERIFY_IS_TRUE(m_pSelection->_srSelectionRect.Bottom == m_pSelection->_srSelectionRect.Top);
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Bottom;

        VerifyGetSelectionRects_LineMode();

        // #2: right to left selection
        m_pSelection->_coordSelectionAnchor.X = m_pSelection->_srSelectionRect.Right;
        VERIFY_IS_TRUE(m_pSelection->_srSelectionRect.Bottom == m_pSelection->_srSelectionRect.Top);
        m_pSelection->_coordSelectionAnchor.Y = m_pSelection->_srSelectionRect.Top;

        VerifyGetSelectionRects_LineMode();
    }

    void TestBisectSelectionDelta(til::CoordType sTargetX, til::CoordType sTargetY, til::CoordType sLength, til::CoordType sDeltaLeft, til::CoordType sDeltaRight)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& screenInfo = gci.GetActiveOutputBuffer();

        til::CoordType sStringLength;
        til::point coordTargetPoint;
        til::inclusive_rect srSelection;
        til::inclusive_rect srOriginal;

        sStringLength = sLength;
        coordTargetPoint.X = sTargetX;
        coordTargetPoint.Y = sTargetY;

        // selection area is always one row at a time so top/bottom = Y = row position
        srSelection.Top = srSelection.Bottom = coordTargetPoint.Y;

        // selection rectangle starts from the target and goes for the length requested
        srSelection.Left = coordTargetPoint.X;
        srSelection.Right = coordTargetPoint.X + sStringLength;

        // save original for comparison
        srOriginal.Top = srSelection.Top;
        srOriginal.Bottom = srSelection.Bottom;
        srOriginal.Left = srSelection.Left;
        srOriginal.Right = srSelection.Right;

        til::point startPos{ sTargetX, sTargetY };
        til::point endPos{ sTargetX + sLength, sTargetY };
        const auto selectionRects = screenInfo.GetTextBuffer().GetTextRects(startPos, endPos, false, false);

        VERIFY_ARE_EQUAL(static_cast<size_t>(1), selectionRects.size());
        srSelection = selectionRects.at(0);

        VERIFY_ARE_EQUAL(srOriginal.Top, srSelection.Top);
        VERIFY_ARE_EQUAL(srOriginal.Bottom, srSelection.Bottom);
        VERIFY_ARE_EQUAL(srOriginal.Left + sDeltaLeft, srSelection.Left);
        VERIFY_ARE_EQUAL(srOriginal.Right + sDeltaRight, srSelection.Right);
    }
};

class SelectionInputTests
{
    TEST_CLASS(SelectionInputTests);

    CommonState* m_state;
    CommandHistory* m_pHistory;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();
        m_pHistory = CommandHistory::s_Allocate(L"cmd.exe", nullptr);
        if (!m_pHistory)
        {
            return false;
        }
        // History must be prepared before COOKED_READ (as it uses s_Find to get at it)

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        CommandHistory::s_Free(nullptr);
        m_pHistory = nullptr;
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD(TestGetInputLineBoundaries)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // 80x80 box
        const til::CoordType sRowWidth = 80;

        til::inclusive_rect srectEdges;
        srectEdges.Left = srectEdges.Top = 0;
        srectEdges.Right = srectEdges.Bottom = sRowWidth - 1;

        // false when no cooked read data exists
        VERIFY_IS_FALSE(gci.HasPendingCookedRead());

        auto fResult = Selection::s_GetInputLineBoundaries(nullptr, nullptr);
        VERIFY_IS_FALSE(fResult);

        // prepare some read data
        m_state->PrepareReadHandle();
        auto cleanupReadHandle = wil::scope_exit([&]() { m_state->CleanupReadHandle(); });

        m_state->PrepareCookedReadData();
        // set up to clean up read data later
        auto cleanupCookedRead = wil::scope_exit([&]() { m_state->CleanupCookedReadData(); });

        auto& readData = gci.CookedReadData();

        // backup text info position over remainder of text execution duration
        auto& textBuffer = gci.GetActiveOutputBuffer().GetTextBuffer();
        til::point coordOldTextInfoPos;
        coordOldTextInfoPos.X = textBuffer.GetCursor().GetPosition().X;
        coordOldTextInfoPos.Y = textBuffer.GetCursor().GetPosition().Y;

        // set various cursor positions
        readData.OriginalCursorPosition().X = 15;
        readData.OriginalCursorPosition().Y = 3;

        readData.VisibleCharCount() = 200;

        textBuffer.GetCursor().SetXPosition(35);
        textBuffer.GetCursor().SetYPosition(35);

        // try getting boundaries with no pointers. parameters should be fully optional.
        fResult = Selection::s_GetInputLineBoundaries(nullptr, nullptr);
        VERIFY_IS_TRUE(fResult);

        // now let's get some actual data
        til::point coordStart;
        til::point coordEnd;

        fResult = Selection::s_GetInputLineBoundaries(&coordStart, &coordEnd);
        VERIFY_IS_TRUE(fResult);

        // starting position/boundary should always be where the input line started
        VERIFY_ARE_EQUAL(coordStart.X, readData.OriginalCursorPosition().X);
        VERIFY_ARE_EQUAL(coordStart.Y, readData.OriginalCursorPosition().Y);

        // ending position can vary. it's in one of two spots
        // 1. If the original cooked cursor was valid (which it was this first time), it's NumberOfVisibleChars ahead.
        til::point coordFinalPos;

        const auto cCharsToAdjust = ((til::CoordType)readData.VisibleCharCount() - 1); // then -1 to be on the last piece of text, not past it

        coordFinalPos.X = (readData.OriginalCursorPosition().X + cCharsToAdjust) % sRowWidth;
        coordFinalPos.Y = readData.OriginalCursorPosition().Y + ((readData.OriginalCursorPosition().X + cCharsToAdjust) / sRowWidth);

        VERIFY_ARE_EQUAL(coordEnd.X, coordFinalPos.X);
        VERIFY_ARE_EQUAL(coordEnd.Y, coordFinalPos.Y);

        // 2. if the original cooked cursor is invalid, then it's the text info cursor position
        readData.OriginalCursorPosition().X = -1;
        readData.OriginalCursorPosition().Y = -1;

        fResult = Selection::s_GetInputLineBoundaries(nullptr, &coordEnd);
        VERIFY_IS_TRUE(fResult);

        VERIFY_ARE_EQUAL(coordEnd.X, textBuffer.GetCursor().GetPosition().X - 1); // -1 to be on the last piece of text, not past it
        VERIFY_ARE_EQUAL(coordEnd.Y, textBuffer.GetCursor().GetPosition().Y);

        // restore text buffer info position
        textBuffer.GetCursor().SetXPosition(coordOldTextInfoPos.X);
        textBuffer.GetCursor().SetYPosition(coordOldTextInfoPos.Y);
    }

    TEST_METHOD(TestWordByWordPrevious)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = gci.GetActiveOutputBuffer();

        const std::wstring text(L"this is some test text.");
        screenInfo.Write(OutputCellIterator(text));

        // Get the left and right side of the text we inserted (right is one past the end)
        const til::point left;
        const til::point right{ gsl::narrow<til::CoordType>(text.length()), 0 };

        // Get the selection instance and buffer size
        auto& sel = Selection::Instance();
        const auto bufferSize = screenInfo.GetBufferSize();

        // The anchor is where the selection started from.
        const auto anchor = right;

        // The point is the "other end" of the anchor forming the rectangle of what is covered.
        // It starts at the same spot as the anchor to represent the initial 1x1 selection.
        auto point = anchor;

        // Walk through the sequence in reverse extending the sequence by one word each time to the left.
        // The anchor is always the end of the line and the selection just gets bigger.
        do
        {
            // We expect the result to be left of where we started.
            // It will point at the character just right of the space (or the beginning of the line).
            auto resultExpected = point;

            do
            {
                resultExpected.X--;
            } while (resultExpected.X > 0 && text.at(resultExpected.X - 1) != UNICODE_SPACE);

            point = sel.WordByWordSelection(true, bufferSize, anchor, point);

            VERIFY_ARE_EQUAL(resultExpected, point);

        } while (point.X > left.X);
    }

    TEST_METHOD(TestWordByWordNext)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = gci.GetActiveOutputBuffer();

        const std::wstring text(L"this is some test text.");
        screenInfo.Write(OutputCellIterator(text));

        // Get the left and right side of the text we inserted (right is one past the end)
        const til::point left;
        const til::point right = { gsl::narrow<til::CoordType>(text.length()), 0 };

        // Get the selection instance and buffer size
        auto& sel = Selection::Instance();
        const auto bufferSize = screenInfo.GetBufferSize();

        // The anchor is where the selection started from.
        const auto anchor = left;

        // The point is the "other end" of the anchor forming the rectangle of what is covered.
        // It starts at the same spot as the anchor to represent the initial 1x1 selection.
        auto point = anchor;

        // Walk through the sequence forward extending the sequence by one word each time to the right.
        // The anchor is always the end of the line and the selection just gets bigger.
        do
        {
            // We expect the result to be right of where we started.

            auto resultExpected = point;

            do
            {
                resultExpected.X++;
            } while (resultExpected.X + 1 < right.X && text.at(resultExpected.X + 1) != UNICODE_SPACE);
            resultExpected.X++;

            // when we reach the end, word by word selection will seek forward to the end of the buffer, so update
            // the expected to the end in that circumstance
            if (resultExpected.X >= right.X)
            {
                resultExpected.X = bufferSize.RightInclusive();
                resultExpected.Y = bufferSize.BottomInclusive();
            }

            point = sel.WordByWordSelection(false, bufferSize, anchor, point);

            VERIFY_ARE_EQUAL(resultExpected, point);

        } while (point.Y < bufferSize.BottomInclusive()); // stop once we've advanced to a point on the bottom row of the buffer.
    }
};
