/**
 * Autogenerated by Thrift Compiler (0.14.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using Thrift;
using Thrift.Collections;
using System.ServiceModel;
using System.Runtime.Serialization;

using Thrift.Protocol;
using Thrift.Protocol.Entities;
using Thrift.Protocol.Utilities;
using Thrift.Transport;
using Thrift.Transport.Client;
using Thrift.Transport.Server;
using Thrift.Processor;


#pragma warning disable IDE0079  // remove unnecessary pragmas
#pragma warning disable IDE1006  // parts of the code use IDL spelling

namespace OptReqDefTest
{

public partial class CrashBoomBang : TException, TBase
{
  private int _MyErrorCode;

  [DataMember(Order = 0)]
  public int MyErrorCode
  {
    get
    {
      return _MyErrorCode;
    }
    set
    {
      __isset.MyErrorCode = true;
      this._MyErrorCode = value;
    }
  }


  [DataMember(Order = 1)]
  public Isset __isset;
  [DataContract]
  public struct Isset
  {
    [DataMember]
    public bool MyErrorCode;
  }

  #region XmlSerializer support

  public bool ShouldSerializeMyErrorCode()
  {
    return __isset.MyErrorCode;
  }

  #endregion XmlSerializer support

  public CrashBoomBang()
  {
  }

  public CrashBoomBang DeepCopy()
  {
    var tmp178 = new CrashBoomBang();
    if(__isset.MyErrorCode)
    {
      tmp178.MyErrorCode = this.MyErrorCode;
    }
    tmp178.__isset.MyErrorCode = this.__isset.MyErrorCode;
    return tmp178;
  }

  public async global::System.Threading.Tasks.Task ReadAsync(TProtocol iprot, CancellationToken cancellationToken)
  {
    iprot.IncrementRecursionDepth();
    try
    {
      TField field;
      await iprot.ReadStructBeginAsync(cancellationToken);
      while (true)
      {
        field = await iprot.ReadFieldBeginAsync(cancellationToken);
        if (field.Type == TType.Stop)
        {
          break;
        }

        switch (field.ID)
        {
          case 1:
            if (field.Type == TType.I32)
            {
              MyErrorCode = await iprot.ReadI32Async(cancellationToken);
            }
            else
            {
              await TProtocolUtil.SkipAsync(iprot, field.Type, cancellationToken);
            }
            break;
          default: 
            await TProtocolUtil.SkipAsync(iprot, field.Type, cancellationToken);
            break;
        }

        await iprot.ReadFieldEndAsync(cancellationToken);
      }

      await iprot.ReadStructEndAsync(cancellationToken);
    }
    finally
    {
      iprot.DecrementRecursionDepth();
    }
  }

  public async global::System.Threading.Tasks.Task WriteAsync(TProtocol oprot, CancellationToken cancellationToken)
  {
    oprot.IncrementRecursionDepth();
    try
    {
      var struc = new TStruct("CrashBoomBang");
      await oprot.WriteStructBeginAsync(struc, cancellationToken);
      var field = new TField();
      if(__isset.MyErrorCode)
      {
        field.Name = "MyErrorCode";
        field.Type = TType.I32;
        field.ID = 1;
        await oprot.WriteFieldBeginAsync(field, cancellationToken);
        await oprot.WriteI32Async(MyErrorCode, cancellationToken);
        await oprot.WriteFieldEndAsync(cancellationToken);
      }
      await oprot.WriteFieldStopAsync(cancellationToken);
      await oprot.WriteStructEndAsync(cancellationToken);
    }
    finally
    {
      oprot.DecrementRecursionDepth();
    }
  }

  public override bool Equals(object that)
  {
    if (!(that is CrashBoomBang other)) return false;
    if (ReferenceEquals(this, other)) return true;
    return ((__isset.MyErrorCode == other.__isset.MyErrorCode) && ((!__isset.MyErrorCode) || (System.Object.Equals(MyErrorCode, other.MyErrorCode))));
  }

  public override int GetHashCode() {
    int hashcode = 157;
    unchecked {
      if(__isset.MyErrorCode)
      {
        hashcode = (hashcode * 397) + MyErrorCode.GetHashCode();
      }
    }
    return hashcode;
  }

  public override string ToString()
  {
    var sb = new StringBuilder("CrashBoomBang(");
    int tmp179 = 0;
    if(__isset.MyErrorCode)
    {
      if(0 < tmp179++) { sb.Append(", "); }
      sb.Append("MyErrorCode: ");
      MyErrorCode.ToString(sb);
    }
    sb.Append(')');
    return sb.ToString();
  }
}


[DataContract]
public partial class CrashBoomBangFault
{
  private int _MyErrorCode;

  [DataMember(Order = 0)]
  public int MyErrorCode
  {
    get
    {
      return _MyErrorCode;
    }
    set
    {
      this._MyErrorCode = value;
    }
  }

}

}
