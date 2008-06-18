#include "ClientInviteSession.hxx"
#include "DialogUsageManager.hxx"
#include "Dialog.hxx"

using namespace resip;

ClientInviteSession::ClientInviteSession(DialogUsageManager& dum, 
                                         Dialog& dialog,
                                         const SipMessage& request, 
                                         const SdpContents* initialOffer)
   : InviteSession(dum, dialog),
     mHandle(dum),
     mLastRequest(request),
     mReceived2xx(false)
{
   mProposedLocalSdp = initialOffer->clone();
}

void 
ClientInviteSession::setOffer(const SdpContents* offer)
{
   
}

void 
ClientInviteSession::sendOfferInAnyMessage()
{
}

void 
ClientInviteSession::setAnswer(const SdpContents* answer)
{
}

void 
ClientInviteSession::sendAnswerInAnyMessage()
{
}

void 
ClientInviteSession::end()
{
   switch (mState)
   {
      case Unknown:
         assert(0);
         break;
         
      case Early:
         // is it legal to cancel a specific fork/dialog. 
         // if so, this is the place to do it
         break;
         
      case Connected:
         InviteSession::end();
         break;
      }
         
      case Terminated:
         // do nothing
         break;
   }
}

void 
ClientInviteSession::rejectOffer(int statusCode)
{
}

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
   if (code < 300 && mState == Unknown)
   {
      handler->onNewSession(getHandle(), msg);
   }
         
   if (code < 200) // 1XX
   {
      if (mState == Unknown || mState == Early)
      {
         mState = Early;
         handler->onEarly(getHandle(), msg);
            
         SdpContents* sdp = dynamic_cast<SdpContents*>(msg.getContents());
         bool reliable = msg.header(h_Supporteds).contains(Symbols::C100rel);
         if (sdp)
         {
            if (reliable)
            {
               if (mProposedLocalSdp)
               {
                  mCurrentRemoteSdp = sdp->clone();
                  mCurrentLocalSdp = mProposedLocalSdp;
                  mProposedLocalSdp = 0;

                  handler->onAnswer(getHandle(), msg);
               }
               else
               {
                  mProposedRemoteSdp = sdp->clone();
                  handler->onOffer(getHandle(), msg);

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
         sendAck();
         end();
         return;
      }
      else if (mState != Terminated)
      {
         mState = Connected;
         if (mReceived2xx) // retransmit ACK
         {
            mDum.send(mAck);
            return;
         }
         
         mReceived2xx = true;
         handler->onConnected(getHandle(), msg);
            
         SdpContents* sdp = dynamic_cast<SdpContents*>(msg.getContents());
         if (sdp)
         {
            if (mProposedLocalSdp) // got an answer
            {
               mCurrentRemoteSdp = sdp->clone();
               mCurrentLocalSdp = mProposedLocalSdp;
               mProposedLocalSdp = 0;
                  
               handler->onAnswer(getHandle(), msg);
            }
            else  // got an offer
            {
               mProposedRemoteSdp = sdp->clone();
               handler->onOffer(getHandle(), msg);
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
         handler->onTerminated(getHandle(), msg);
      }
   }
   else // 3xx
   {
      assert(0);
   }
}

void
ClientInviteSession::sendPrack(const SipMessage& response)
{
   assert(response.isResponse());
   assert(response.header(h_StatusLine).statusCode() > 100 && 
          response.header(h_StatusLine).statusCode() < 200);
   
   SipMessage prack;
   mDialog.makePrack(prack);

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
   mDialog.makeAck(mAck);
   if (mProposedLocalSdp)
   {
      mDialog.setContents(mProposedLocalSdp);
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