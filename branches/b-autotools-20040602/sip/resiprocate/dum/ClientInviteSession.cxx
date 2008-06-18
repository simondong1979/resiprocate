#include "resiprocate/SdpContents.hxx"
#include "resiprocate/dum/ClientInviteSession.hxx"
#include "resiprocate/dum/Dialog.hxx"
#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/InviteSessionHandler.hxx"
#include "resiprocate/dum/DumTimeout.hxx"
#include "resiprocate/os/Logger.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::DUM

ClientInviteSession::ClientInviteSession(DialogUsageManager& dum, 
                                         Dialog& dialog,
                                         const SipMessage& request, 
                                         const SdpContents* initialOffer)
   : InviteSession(dum, dialog),
     lastReceivedRSeq(0),
     lastExpectedRSeq(0),
     mStaleCallTimerSeq(1)
{
   assert(request.isRequest());
   if (initialOffer)
   {
      sendSdp(initialOffer);
   }
   mLastRequest = request;
}

void
ClientInviteSession::dispatch(const SipMessage& msg)
{
   std::pair<OfferAnswerType, const SdpContents*> offans;
   offans = InviteSession::getOfferOrAnswer(msg);
   
   // !jf! consider UPDATE method

   switch(mState)
   {
      case Initial:
      {
         assert(msg.isResponse());
         int code = msg.header(h_StatusLine).statusCode();
         if (code == 100)
         {
            mDum.addTimer(DumTimeout::StaleCall, DumTimeout::StaleCallTimeout, getBaseHandle(), mStaleCallTimerSeq);
            mState = Proceeding;
            mDum.mInviteSessionHandler->onNewSession(getHandle(), None, msg);
         }
         else if (code < 200)
         {
            mDum.addTimer(DumTimeout::StaleCall, DumTimeout::StaleCallTimeout, getBaseHandle(), mStaleCallTimerSeq);
            mState = Early;
            mDum.mInviteSessionHandler->onNewSession(getHandle(), offans.first, msg);
            mDum.mInviteSessionHandler->onProvisional(getHandle(), msg);
            
            if (offans.first != None)
            {
               InviteSession::incomingSdp(msg, offans.second);
            }
            else if (offans.second)
            {
               mDum.mInviteSessionHandler->onEarlyMedia(getHandle(), msg, offans.second);
            }
         }
         else if (code < 300)
         {
            mDum.addTimer(DumTimeout::StaleCall, DumTimeout::StaleCallTimeout, getBaseHandle(), mStaleCallTimerSeq);
            mState = Connected;
            mDum.mInviteSessionHandler->onNewSession(getHandle(), offans.first, msg);
            mDum.mInviteSessionHandler->onConnected(getHandle(), msg);
            
            if (offans.first != None)
            {
               InviteSession::incomingSdp(msg, offans.second);
            }
            sendAck(msg);
         }
         else if (code >= 300)
         {
            mDum.mInviteSessionHandler->onTerminated(getSessionHandle(), msg);
            delete this;
         }
         break;
      }
      
      case Proceeding:
      case Early:
      {
         assert(msg.isResponse());
         int code = msg.header(h_StatusLine).statusCode();
         if (code == 100)
         {
         }
         else if (code < 200)
         {
            mDum.addTimer(DumTimeout::StaleCall, DumTimeout::StaleCallTimeout, getBaseHandle(), ++mStaleCallTimerSeq);
            mState = Early;
            mDum.mInviteSessionHandler->onProvisional(getHandle(), msg);
            
            if (offans.first != None)
            {
               InviteSession::incomingSdp(msg, offans.second);
            }
            else if (offans.second)
            {
               mDum.mInviteSessionHandler->onEarlyMedia(getHandle(), msg, offans.second);
            }
         }
         else if (code < 300)
         {
            mDum.addTimer(DumTimeout::StaleCall, DumTimeout::StaleCallTimeout, getBaseHandle(), mStaleCallTimerSeq);
            mState = Connected;
            mDum.mInviteSessionHandler->onNewSession(getHandle(), offans.first, msg);
            mDum.mInviteSessionHandler->onConnected(getHandle(), msg);
            
            if (offans.first != None)
            {
               InviteSession::incomingSdp(msg, offans.second);
            }
            sendAck(msg);
         }
         else if (code >= 300)
         {
            mDum.mInviteSessionHandler->onTerminated(getSessionHandle(), msg);
            delete this;
         }
         break;
      }
      
      case Cancelled:
      {
         if (msg.isResponse())
         {
            int code = msg.header(h_StatusLine).statusCode();
            if (code / 100 == 2 && msg.header(h_CSeq).method() == INVITE)
            {
               end();
            }
            else if (code >= 300 && msg.header(h_CSeq).method() == INVITE)
            {
               delete this;
            }
         }
         break;
      }
         
      case Connected:
         InviteSession::dispatch(msg);
         break;
         
      case Terminated:
         if (msg.isResponse() && msg.header(h_StatusLine).statusCode() == 200 && msg.header(h_CSeq).method() == BYE)
         {
            delete this;
         }
         break;
      }
}

void
ClientInviteSession::dispatch(const DumTimeout& timeout)
{
   
}

void
ClientInviteSession::send(const SipMessage& msg)
{
   if (msg.header(h_CSeq).method() == BYE && 
       msg.isResponse() && msg.header(h_StatusLine).statusCode() == 200)       
   {
      mState = Terminated;
   }
   InviteSession::send(msg);
}

SipMessage&
ClientInviteSession::end()
{
   switch (mState)
   {
      case Unknown:
         assert(0);
         return mLastRequest;
         
      case Early:
         // is it legal to cancel a specific fork/dialog. 
         // if so, this is the place to do it
         assert(0);
         return mLastRequest;
         
      case Connected:
         return InviteSession::end();
         break;
         
      case Terminated:
         // do nothing
         assert(0);
         return mLastRequest;         

      case Cancelled:
         // !jf! 
         assert(0);
         return mLastRequest;         
         
      default:
         return mLastRequest;
   }
}

SipMessage&
ClientInviteSession::rejectOffer(int statusCode)
{
   return mLastRequest;
}

#if 0
void
ClientInviteSession::dispatch(const SipMessage& msg)
{
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   assert(handler);
   
   if (msg.isRequest())
   {
      InviteSession::dispatch(msg);
      return;
   }
   else if (msg.isResponse())
   {
      switch (msg.header(h_CSeq).method())
      {
         case INVITE:
            break;
            
         case PRACK:
            handlePrackResponse(msg);
            return;
            
         case CANCEL:
            if (msg.header(h_StatusLine).statusCode() >= 400)
            {
               mState = Terminated;
               end(); // cleanup the mess
            }
            return;            
            
         default:
            InviteSession::dispatch(msg);
            return;
      }
   }
   
   int code = msg.header(h_StatusLine).statusCode();
   if (code < 300 && mState == Initial)
   {
      //handler->onNewSession(getHandle(), msg);
   }
         
   if (code < 200) // 1XX
   {
      if (mState == Initial || mState == Early)
      {
         mState = Early;
         //handler->onEarly(getHandle(), msg);
            
         SdpContents* sdp = dynamic_cast<SdpContents*>(msg.getContents());
         bool reliable = msg.header(h_Supporteds).find(Token(Symbols::C100rel));
         if (sdp)
         {
            if (reliable)
            {
               if (mProposedLocalSdp)
               {
                  mCurrentRemoteSdp = static_cast<SdpContents*>(sdp->clone());
                  mCurrentLocalSdp = mProposedLocalSdp;
                  mProposedLocalSdp = 0;

                  //handler->onAnswer(getHandle(), msg);
               }
               else
               {
                  mProposedRemoteSdp = static_cast<SdpContents*>(sdp->clone());
                  handler->onOffer(getSessionHandle(), msg);

                  // handler must provide an answer
                  assert(mProposedLocalSdp);
               }
            }
            else
            {
               // do nothing, not an offer/answer
            }
         }
         if (reliable)
         {
            sendPrack(msg);
         }
      }
      else
      {
         // drop it on the floor. Late 1xx
      }
   }
   else if (code < 300) // 2XX
   {
      if (mState == Cancelled)
      {
         //sendAck(the200);  
         end();
         return;
      }
      else if (mState != Terminated)
      {
         mState = Connected;
         // !jf!
         //if (mReceived2xx) // retransmit ACK
         {
            mDum.send(mAck);
            return;
         }
         
         //mReceived2xx = true;
         handler->onConnected(getHandle(), msg);
            
         SdpContents* sdp = dynamic_cast<SdpContents*>(msg.getContents());
         if (sdp)
         {
            if (mProposedLocalSdp) // got an answer
            {
               mCurrentRemoteSdp = static_cast<SdpContents*>(sdp->clone());
               mCurrentLocalSdp = mProposedLocalSdp;
               mProposedLocalSdp = 0;
                  
               //handler->onAnswer(getHandle(), msg);
            }
            else  // got an offer
            {
               mProposedRemoteSdp = static_cast<SdpContents*>(sdp->clone());
               handler->onOffer(getSessionHandle(), msg);
            }
         }
         else
         {
            if (mProposedLocalSdp)
            {
               // Got a 2xx with no answer (sent an INVITE with an offer,
               // unreliable provisionals)
               end();
               return;
            }
            else if (mCurrentLocalSdp == 0 && mProposedRemoteSdp == 0)
            {
               // Got a 2xx with no offer (sent an INVITE with no offer,
               // unreliable provisionals)
               end();
               return;
            }
            else
            {
               assert(mCurrentLocalSdp != 0);
               // do nothing
            }
         }
         sendAck(msg);
      }
   }
   else if (code >= 400)
   {
      if (mState != Terminated)
      {
         mState = Terminated;
         handler->onTerminated(getSessionHandle(), msg);
      }
   }
   else // 3xx
   {
      assert(0);
   }
}
#endif

void
ClientInviteSession::sendPrack(const SipMessage& response)
{
   assert(response.isResponse());
   assert(response.header(h_StatusLine).statusCode() > 100 && 
          response.header(h_StatusLine).statusCode() < 200);
   
   SipMessage prack;
   mDialog.makeRequest(prack, PRACK);
   
   if (mProposedRemoteSdp)
   {
      assert(mProposedLocalSdp);
      // send an answer
      prack.setContents(mProposedLocalSdp);
      
   }
   else if (mProposedLocalSdp)
   {
      // send a counter-offer
      prack.setContents(mProposedRemoteSdp);
   }
   else
   {
      // no sdp
   }
   
   // much later!!! the deep rathole ....
   // if there is a pending offer or answer, will include it in the PRACK body
   assert(0);

   
}

void
ClientInviteSession::handlePrackResponse(const SipMessage& response)
{
   // more PRACK goodness 
   assert(0);
}

void
ClientInviteSession::sendAck(const SipMessage& ok)
{
   makeAck(ok);
   if (mProposedLocalSdp)
   {
      // !jf! ?
      //mDialog.setContents(mProposedLocalSdp);
   }
   mDum.send(mAck);
}


ClientInviteSession::Handle::Handle(DialogUsageManager& dum)
   : BaseUsage::Handle(dum)
{}

ClientInviteSession* 
ClientInviteSession::Handle::operator->()
{
   return static_cast<ClientInviteSession*>(get());
}

InviteSession::Handle 
ClientInviteSession::getSessionHandle()
{
   // don't ask, don't tell
   return (InviteSession::Handle&)mHandle;
}


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the

 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */